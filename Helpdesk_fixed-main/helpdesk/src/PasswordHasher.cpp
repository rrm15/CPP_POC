#include "PasswordHasher.h"
#include "Common.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <vector>
#include <cctype>
#include <cstring>

namespace {

inline int hexCharToInt(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string toHex(const unsigned char* data, size_t len) {
    static const char hexDigits[] = "0123456789abcdef";
    std::string hex;
    hex.resize(len * 2);
    for (size_t i = 0; i < len; ++i) {
        hex[i * 2]     = hexDigits[(data[i] >> 4) & 0x0F];
        hex[i * 2 + 1] = hexDigits[data[i] & 0x0F];
    }
    return hex;
}

bool fromHex(const std::string& hex, unsigned char* out, size_t expectedLen) {
    if (hex.length() != expectedLen * 2) return false;
    for (size_t i = 0; i < expectedLen; ++i) {
        int hi = hexCharToInt(hex[i * 2]);
        int lo = hexCharToInt(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<unsigned char>((hi << 4) | lo);
    }
    return true;
}

bool splitEncoded(const std::string& encodedValue, std::vector<std::string>& parts) {
    parts.clear();
    size_t start = 0;
    while (start <= encodedValue.length()) {
        size_t pos = encodedValue.find('$', start);
        if (pos == std::string::npos) {
            parts.push_back(encodedValue.substr(start));
            break;
        }
        parts.push_back(encodedValue.substr(start, pos - start));
        start = pos + 1;
    }
    return parts.size() == 4;
}

} // anonymous namespace

std::string PasswordHasher::hashPassword(const std::string& plaintext) {
    unsigned char salt[SALT_BYTES];
    if (RAND_bytes(salt, SALT_BYTES) != 1) {
        throw AppException("Cryptographic failure: unable to generate secure random salt.");
    }

    unsigned char hash[HASH_BYTES];
    int rc = PKCS5_PBKDF2_HMAC(
        plaintext.c_str(),
        static_cast<int>(plaintext.length()),
        salt,
        SALT_BYTES,
        DEFAULT_ITERATIONS,
        EVP_sha256(),
        HASH_BYTES,
        hash
    );

    if (rc != 1) {
        throw AppException("Cryptographic failure: PBKDF2-HMAC-SHA256 key derivation failed.");
    }

    std::string saltHex = toHex(salt, SALT_BYTES);
    std::string hashHex = toHex(hash, HASH_BYTES);

    return "pbkdf2_sha256$" + std::to_string(DEFAULT_ITERATIONS) + "$" + saltHex + "$" + hashHex;
}

bool PasswordHasher::verifyPassword(const std::string& plaintext, const std::string& encodedValue) {
    std::vector<std::string> parts;
    if (!splitEncoded(encodedValue, parts)) {
        return false;
    }

    // 1. Algorithm identifier validation
    if (parts[0] != "pbkdf2_sha256") {
        return false;
    }

    // 2. Iteration count validation (overflow-safe, strictly numeric, non-zero, within bounds)
    if (parts[1].empty()) {
        return false;
    }
    for (char c : parts[1]) {
        if (c < '0' || c > '9') return false;
    }
    unsigned long iterations = 0;
    try {
        size_t idx = 0;
        iterations = std::stoul(parts[1], &idx);
        if (idx != parts[1].length() || iterations == 0 || iterations > 10000000UL) {
            return false;
        }
    } catch (...) {
        return false;
    }

    // 3. Salt field validation and decoding
    unsigned char salt[SALT_BYTES];
    if (!fromHex(parts[2], salt, SALT_BYTES)) {
        return false;
    }

    // 4. Stored hash field validation and decoding
    unsigned char expectedHash[HASH_BYTES];
    if (!fromHex(parts[3], expectedHash, HASH_BYTES)) {
        return false;
    }

    // 5. Derive key using parsed parameters
    unsigned char computedHash[HASH_BYTES];
    int rc = PKCS5_PBKDF2_HMAC(
        plaintext.c_str(),
        static_cast<int>(plaintext.length()),
        salt,
        SALT_BYTES,
        static_cast<int>(iterations),
        EVP_sha256(),
        HASH_BYTES,
        computedHash
    );

    if (rc != 1) {
        return false;
    }

    // 6. Constant-time comparison to prevent timing attacks
    return CRYPTO_memcmp(computedHash, expectedHash, HASH_BYTES) == 0;
}
