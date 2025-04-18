#ifndef AUTH_UTILS_H
#define AUTH_UTILS_H

#include <Arduino.h>

/**
 * @namespace AuthUtils
 * @brief Provides utility functions for secure password handling and data conversion.
 *
 * This namespace contains functions for generating salts, hashing passwords using
 * SHA-256 with salts, verifying passwords against stored hashes, and converting
 * between byte arrays and their hexadecimal string representations.
 */
namespace AuthUtils {

    // Generates a cryptographically secure random salt
    /**
     * @brief Generates a cryptographically secure random salt.
     *
     * Uses the ESP32's hardware random number generator to produce a salt of the
     * specified byte length.
     *
     * @param length The desired length of the salt in bytes. Defaults to 16 bytes.
     * @return A String containing the hexadecimal representation of the generated salt
     *         (e.g., 32 hex characters for a 16-byte salt). Returns an empty string
     *         on error (e.g., memory allocation failure).
     */
    // Returns a hex-encoded string representation of the salt (e.g., 32 hex chars for 16 bytes)
    String generateSalt(size_t length = 16); // Default to 16 bytes (32 hex chars)

    // Hashes a password using SHA-256 with the provided salt.
    /**
     * @brief Hashes a password using SHA-256 combined with a provided salt.
     *
     * Concatenates the salt (from hex) and the password, then computes the SHA-256 hash.
     *
     * @param password The plain text password String to hash.
     * @param saltHex The hexadecimal String representation of the salt to use.
     * @return A String containing the hexadecimal representation of the SHA-256 hash
     *         (64 hex characters). Returns an empty string on error (e.g., invalid salt,
     *         memory allocation failure).
     */
    // Input: plain password string, hex-encoded salt string.
    // Output: hex-encoded SHA-256 hash string (64 hex chars).
    // Returns an empty string on error.
    String hashPassword(const String& password, const String& saltHex);

    // Verifies a given password against a stored hash and salt.
    /**
     * @brief Verifies a given plain text password against a stored hash and salt.
     *
     * Re-hashes the provided password using the given salt and compares the result
     * to the stored hash.
     *
     * @param password The plain text password String to verify.
     * @param storedHashHex The hexadecimal String representation of the hash stored previously.
     * @param saltHex The hexadecimal String representation of the salt used with the stored hash.
     * @return True if the calculated hash matches the stored hash, false otherwise
     *         (including errors during hashing).
     */
    // Input: plain password string, hex-encoded stored hash string, hex-encoded salt string.
    // Output: true if the password matches the hash, false otherwise.
    bool verifyPassword(const String& password, const String& storedHashHex, const String& saltHex);

    // Helper function to convert byte array to hex string
    /**
     * @brief Converts a byte array to its hexadecimal string representation.
     *
     * @param bytes Pointer to the constant byte array to convert.
     * @param len The number of bytes in the array.
     * @return A String containing the hexadecimal representation.
     */
    String bytesToHex(const unsigned char* bytes, size_t len);

    // Helper function to convert hex string to byte array
    // Returns the number of bytes converted, or 0 on error.
    /**
     * @brief Converts a hexadecimal string representation back into a byte array.
     *
     * @param hex The hexadecimal String to convert. Must have an even number of characters.
     * @param outBytes Pointer to the output byte array where the result will be stored.
     *                 Must be pre-allocated with sufficient size (`hex.length() / 2`).
     * @param maxLen The maximum number of bytes the output buffer `outBytes` can hold.
     * @return The number of bytes written to the output buffer, or 0 if the input
     *         hex string is invalid (odd length) or `maxLen` is insufficient.
     */
    // Output buffer 'outBytes' must be pre-allocated with sufficient size (hexLen / 2).
    size_t hexToBytes(const String& hex, unsigned char* outBytes, size_t maxLen);

} // namespace AuthUtils

#endif // AUTH_UTILS_H