#ifndef PASSWORD_HASHER_H
#define PASSWORD_HASHER_H

#include <string>

// -----------------------------------------------------------------------
// PasswordHasher
// -----------------------------------------------------------------------
// PBKDF2-HMAC-SHA256 password hashing and verification using OpenSSL
// libcrypto.
//
// Stored format:
//   pbkdf2_sha256$<iterations>$<salt-hex>$<hash-hex>
//
// Complies with NIST SP 800-132 and OWASP password storage guidelines:
// - Named iteration-count constant (default: 100,000 iterations).
// - Fresh 16-byte cryptographically secure random salt (RAND_bytes) per hash.
// - 32-byte (256-bit) PBKDF2-HMAC-SHA256 key derivation.
// - Constant-time verification via CRYPTO_memcmp to prevent timing attacks.
// - Strict format and field validation; safe handling of malformed hashes.
// -----------------------------------------------------------------------
class PasswordHasher {
public:
    static constexpr int DEFAULT_ITERATIONS = 100000;
    static constexpr int SALT_BYTES = 16;
    static constexpr int HASH_BYTES = 32;

    // Hashes a plaintext password using PBKDF2-HMAC-SHA256 with a fresh random salt.
    // Returns the encoded string in "pbkdf2_sha256$<iterations>$<salt-hex>$<hash-hex>" format.
    static std::string hashPassword(const std::string& plaintext);

    // Verifies a plaintext password against an encoded hash value.
    // Returns true if the password matches, false if incorrect or if encodedValue is malformed.
    static bool verifyPassword(const std::string& plaintext, const std::string& encodedValue);
};

#endif // PASSWORD_HASHER_H
