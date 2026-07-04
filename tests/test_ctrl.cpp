// Self-test for ctrl protocol + pairing gate + LAN check + secure token.
//   builds with vcpkg nlohmann-json; links bcrypt for secureToken().
// Asserts only, exits non-zero on failure.
#include "../src/network/ctrl_protocol.hpp"
#include "../src/network/pairing.hpp"
#include "../src/network/lan.hpp"
#include "../src/network/secure_token.hpp"
#include <cassert>
#include <cstdio>
#include <string>

using namespace idsp;

static void test_ctrl_parse() {
    auto m = ctrl::parse(R"({"type":"PAIR_REQUEST","deviceId":"A1"})");
    assert(m && m->type == ctrl::Type::PairRequest && !m->token);

    auto m2 = ctrl::parse(R"({"type":"PING","sessionToken":"tok","ts":1})");
    assert(m2 && m2->type == ctrl::Type::Ping && m2->token && *m2->token == "tok");

    assert(!ctrl::parse("not json"));         // malformed → nullopt (no throw)
    assert(!ctrl::parse("[1,2,3]"));          // not an object → nullopt

    auto pr = ctrl::asPairRequest(R"({"type":"PAIR_REQUEST","deviceName":"iPad","deviceId":"X9"})");
    assert(pr && pr->deviceId == "X9" && pr->deviceName == "iPad");
    assert(!ctrl::asPairRequest(R"({"type":"PAIR_REQUEST"})"));  // missing deviceId

    auto sp = ctrl::asSetPrefs(R"({"type":"SET_PREFS","fps":30,"display":{"letterbox":false}})");
    assert(sp && sp->fps == 30 && sp->letterbox == false);
    auto sp2 = ctrl::asSetPrefs(R"({"type":"SET_PREFS"})");
    assert(sp2 && sp2->fps == 0 && sp2->letterbox == true);  // defaults
}

static void test_ctrl_build() {
    auto line = ctrl::buildPairAccept("abc123");
    assert(!line.empty() && line.back() == '\n');
    auto back = ctrl::parse(line);
    assert(back && back->type == ctrl::Type::PairAccept && back->token && *back->token == "abc123");

    auto ack = ctrl::parse(ctrl::buildConfigAck("t", "extend", "16:9", 1920, 1080, 60));
    assert(ack && ack->type == ctrl::Type::ConfigAck);

    auto idle = ctrl::parse(ctrl::buildStreamState("t", true));
    assert(idle && idle->type == ctrl::Type::StreamIdle);
}

static void test_pairing_gate() {
    uint64_t now = 0;
    PairingManager pm(/*timeoutMs=*/1000,
                      [] { return std::string("FIXED_TOKEN"); },
                      [&now] { return now; });

    // deny-by-default before any pairing
    assert(pm.state() == PairState::Idle);
    assert(!pm.validate("anything"));

    // request → awaiting; second request rejected (one client)
    assert(pm.onRequest("dev1"));
    assert(pm.state() == PairState::AwaitingApproval);
    assert(!pm.onRequest("dev2"));

    // accept → paired + token gates access
    std::string tok = pm.accept();
    assert(tok == "FIXED_TOKEN" && pm.state() == PairState::Paired);
    assert(pm.validate("FIXED_TOKEN"));
    assert(!pm.validate("wrong"));

    // deny/reset → back to deny-by-default
    pm.deny();
    assert(pm.state() == PairState::Idle && !pm.validate("FIXED_TOKEN"));
}

static void test_pairing_timeout() {
    uint64_t now = 0;
    PairingManager pm(1000, [] { return std::string("T"); }, [&now] { return now; });
    assert(pm.onRequest("dev"));
    now = 1000;                       // reach timeout
    assert(pm.expireIfNeeded());
    assert(pm.state() == PairState::Idle);
    assert(pm.accept().empty());      // can't accept an expired prompt
}

static void test_lan() {
    using namespace idsp::lan;
    assert(parseIPv4("192.168.1.10") == ((192u<<24)|(168u<<16)|(1u<<8)|10u));
    assert(!parseIPv4("256.0.0.1"));
    assert(!parseIPv4("1.2.3"));
    assert(!parseIPv4("1.2.3.4.5"));
    assert(!parseIPv4("a.b.c.d"));
    assert(!parseIPv4("1.2.3."));

    assert(isPrivateIPv4(*parseIPv4("192.168.0.1")));
    assert(isPrivateIPv4(*parseIPv4("10.255.1.1")));
    assert(isPrivateIPv4(*parseIPv4("172.16.0.1")));
    assert(isPrivateIPv4(*parseIPv4("172.31.255.255")));
    assert(!isPrivateIPv4(*parseIPv4("172.32.0.1")));   // just outside /12
    assert(!isPrivateIPv4(*parseIPv4("8.8.8.8")));
    assert(isPrivateIPv4(*parseIPv4("127.0.0.1")));
    assert(isPrivateIPv4(*parseIPv4("169.254.5.5")));

    uint32_t mask = 0xFFFFFF00u;  // /24
    assert(sameSubnet(*parseIPv4("192.168.1.50"), *parseIPv4("192.168.1.1"), mask));
    assert(!sameSubnet(*parseIPv4("192.168.2.50"), *parseIPv4("192.168.1.1"), mask));
}

static void test_secure_token() {
    std::string a = secureToken(16);
    std::string b = secureToken(16);
    assert(a.size() == 32 && b.size() == 32);
    for (char c : a) assert((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    assert(a != b);  // CSPRNG → collisions astronomically unlikely
}

int main() {
    test_ctrl_parse();
    test_ctrl_build();
    test_pairing_gate();
    test_pairing_timeout();
    test_lan();
    test_secure_token();
    std::puts("ctrl/pairing/lan/token self-test: ALL PASS");
    return 0;
}
