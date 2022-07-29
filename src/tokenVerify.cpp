#include "../lib/sodium.h"
#include <string>
#include <time.h>
#include <unordered_map>
#include <windows.h>
using namespace std;
extern unordered_map<string, string> knownAppNames;
unsigned char pk[32] = {
    0xd7, 0x9b, 0xf0, 0xac, 0xf4, 0xe7, 0x44, 0x9b,
    0x25, 0x5b, 0x53, 0xf6, 0xac, 0xf9, 0xe2, 0xdb,
    0xd8, 0x5d, 0xa4, 0x33, 0xaa, 0x78, 0xd6, 0x4b,
    0x03, 0x5d, 0xfe, 0x1c, 0xb3, 0x46, 0x34, 0xe1};
bool verifyToken(string token, string origin, string sign) {
    // starts with @ && in whitelist
    if (origin.find_first_of("@") == 0 && knownAppNames.find(origin) != knownAppNames.end()) {
        return true;
    }
    // extract sign to buffer
    unsigned char buffer[70];
    if (sodium_hex2bin(buffer, 70, sign.c_str(), sign.length(), NULL, NULL, NULL) != 0) {
        return false;
    }
    // check if starts with 0x99 0x01
    if (buffer[0] != 0x99 || buffer[1] != 0x01) {
        return false;
    }
    // 4-bytes timestamp
    unsigned int expiresAt = buffer[2] << 24 | buffer[3] << 16 | buffer[4] << 8 | buffer[5];
    // check if expired
    if (expiresAt < time(NULL)) {
        return false;
    }
    // get sig message
    string sigMessage = to_string(expiresAt) + "&" + token + "&" + origin;
    // verify signature
    int res = crypto_sign_ed25519_verify_detached(&buffer[6], (unsigned char *)sigMessage.c_str(), sigMessage.length(), pk);
    if (res != 0) {
        return false;
    }
    return true;
}
bool verifyToken(unordered_map<string, string> &args) {
    if (
        args.find("register-token") != args.end() &&
        args.find("register-origin") != args.end() &&
        verifyToken(
            args["register-token"],
            args["register-origin"],
            args.find("register-sign") != args.end() ? args["register-sign"] : "")) {
        return true;
    }
    return false;
}