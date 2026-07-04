#include "ctrl_server.hpp"
#include "lan.hpp"
#include "secure_token.hpp"
#include <chrono>
#include <string>

namespace idsp {

static uint64_t nowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

CtrlServer::CtrlServer(uint16_t port, PromptFn prompt, PrefsFn onPrefs, AckInfoFn ackInfo)
    : m_port(port), m_prompt(std::move(prompt)), m_onPrefs(std::move(onPrefs)),
      m_ackInfo(std::move(ackInfo)),
      m_pairing(/*promptTimeoutMs=*/30000, [] { return secureToken(16); }, nowMs) {}

CtrlServer::~CtrlServer() { stop(); }

bool CtrlServer::start() {
    m_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listen == INVALID_SOCKET) return false;
    int yes = 1;
    setsockopt(m_listen, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&yes), sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(m_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (::bind(m_listen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) return false;
    if (listen(m_listen, 1) != 0) return false;
    m_running.store(true, std::memory_order_release);
    m_thread = std::thread(&CtrlServer::loop, this);
    return true;
}

void CtrlServer::stop() {
    m_running.store(false, std::memory_order_release);
    if (m_listen != INVALID_SOCKET) { closesocket(m_listen); m_listen = INVALID_SOCKET; }
    if (m_thread.joinable()) m_thread.join();
}

void CtrlServer::loop() {
    while (m_running.load(std::memory_order_acquire)) {
        sockaddr_in from{}; int fromLen = sizeof(from);
        SOCKET client = accept(m_listen, reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (client == INVALID_SOCKET) continue;

        // R5: reject non-LAN peers outright.
        if (!lan::isPrivateIPv4(ntohl(from.sin_addr.s_addr))) { closesocket(client); continue; }

        char ipbuf[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &from.sin_addr, ipbuf, sizeof(ipbuf));
        serveClient(client, ipbuf);
        closesocket(client);
        m_pairing.reset();               // one client at a time; clean slate for the next
        if (m_onUnpaired) m_onUnpaired();  // stop streaming when the client goes away
    }
}

// Send a newline-delimited line. Returns false on socket error.
static bool sendLine(SOCKET s, const std::string& line) {
    return send(s, line.c_str(), static_cast<int>(line.size()), 0) != SOCKET_ERROR;
}

void CtrlServer::serveClient(SOCKET client, const std::string& clientIp) {
    std::string acc;                 // line accumulator
    char buf[2048];
    while (m_running.load(std::memory_order_acquire)) {
        int n = recv(client, buf, sizeof(buf), 0);
        if (n <= 0) return;          // client closed / error
        acc.append(buf, static_cast<size_t>(n));

        size_t nl;
        while ((nl = acc.find('\n')) != std::string::npos) {
            std::string line = acc.substr(0, nl);
            acc.erase(0, nl + 1);
            if (line.empty()) continue;

            auto msg = ctrl::parse(line);
            if (!msg) continue;      // malformed → ignore (R2)

            switch (msg->type) {
                case ctrl::Type::PairRequest: {
                    auto req = ctrl::asPairRequest(line);
                    if (!req) break;
                    if (!m_pairing.onRequest(req->deviceId)) {   // busy → deny
                        sendLine(client, ctrl::buildPairDeny("busy"));
                        break;
                    }
                    // blocking Accept/Deny prompt (tray). ponytail: modal prompt; async toast later.
                    bool accepted = m_prompt && m_prompt(req->deviceName, req->deviceId);
                    if (accepted) {
                        std::string tok = m_pairing.accept();   // may be empty if timed out
                        if (tok.empty()) { sendLine(client, ctrl::buildPairDeny("timeout")); break; }
                        sendLine(client, ctrl::buildPairAccept(tok));
                        AckParams a = m_ackInfo ? m_ackInfo() : AckParams{};
                        sendLine(client, ctrl::buildConfigAck(tok, a.mode, a.aspect,
                                                              a.width, a.height, a.fps));
                        if (m_onPaired) m_onPaired(clientIp);  // begin video stream to this peer
                    } else {
                        m_pairing.deny();
                        sendLine(client, ctrl::buildPairDeny("user_denied"));
                    }
                    break;
                }
                case ctrl::Type::SetPrefs: {
                    if (!msg->token || !m_pairing.validate(*msg->token)) break;  // gate (R6)
                    if (auto p = ctrl::asSetPrefs(line); p && m_onPrefs) m_onPrefs(*p);
                    break;
                }
                case ctrl::Type::Ping: {
                    if (msg->token && m_pairing.validate(*msg->token))
                        sendLine(client, ctrl::buildPing(*msg->token, nowMs()));  // reuse as PONG-ish
                    break;
                }
                case ctrl::Type::Disconnect:
                    return;
                default:
                    break;
            }
        }
    }
}

}  // namespace idsp
