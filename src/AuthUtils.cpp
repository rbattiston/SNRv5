#include "AuthUtils.h"
#include "esp_random.h" // For secure random number generation
#include "mbedtls/sha256.h" // For SHA-256 hashing
#include <cstring> // For memcpy

namespace AuthUtils {

    /**
     * @brief Converts a byte array to its hexadecimal string representation.
     *
     * Iterates through the byte array and converts each byte into a two-character
     * hexadecimal string, appending it to the result string.
     *
     * @param bytes Pointer to the constant byte array to convert.
     * @param len The number of bytes in the array.
     * @return A String containing the hexadecimal representation of the byte array.
     */
    // Helper function to convert byte array to hex string
    String bytesToHex(const unsigned char* bytes, size_t len) {
        String hexStr = "";
        hexStr.reserve(len * 2); // Pre-allocate string size
        for (size_t i = 0; i < len; ++i) {
            char hex[3];
            sprintf(hex, "%02x", bytes[i]);
            hexStr += hex;
        }
        return hexStr;
    }

    /**
     * @brief Converts a hexadecimal string representation back into a byte array.
     *
     * Parses the hexadecimal string two characters at a time, converting each pair
     * into a byte and storing it in the output buffer.
     *
     * @param hex The hexadecimal String to convert. Must have an even number of characters.
     * @param outBytes Pointer to the output byte array where the result will be stored.
     * @param maxLen The maximum number of bytes the output buffer can hold.
     * @return The number of bytes written to the output buffer, or 0 if the input
     *         hex string is invalid (odd length) or the buffer is too small.
     */
    // Helper function to convert hex string to byte array
    size_t hexToBytes(const String& hex, unsigned char* outBytes, size_t maxLen) {
        size_t hexLen = hex.length();
        if (hexLen % 2 != 0 || maxLen < hexLen / 2) {
            return 0; // Invalid hex string length or insufficient buffer size
        }

        size_t byteLen = hexLen / 2;
        for (size_t i = 0; i < byteLen; ++i) {
            char hexPair[3] = { hex[i * 2], hex[i * 2 + 1], '\0' };
            outBytes[i] = (unsigned char)strtol(hexPair, NULL, 16);
        }
        return byteLen;
    }


    /**
     * @brief Generates a cryptographically secure random salt of a specified length.
     *
     * Uses the ESP32's hardware random number generator (`esp_fill_random`) to create
     * a byte array of the specified length, then converts it to a hexadecimal string.
     *
     * @param length The desired length of the salt in bytes.
     * @return A String containing the hexadecimal representation of the generated salt,
     *         or an empty string if length is 0 or memory allocation fails.
     */
    String generateSalt(size_t length) {
        if (length == 0) return "";
        unsigned char* saltBytes = new unsigned char[length];
        if (!saltBytes) return ""; // Allocation failed

        esp_fill_random(saltBytes, length); // Fill with random bytes

        String saltHex = bytesToHex(saltBytes, length);
        delete[] saltBytes; // Clean up allocated memory
        return saltHex;
    }

    /**
     * @brief Hashes a password using SHA-256 combined with a provided salt.
     *
     * Converts the hexadecimal salt string back to bytes, concatenates the salt
     * bytes and the password string (salt + password), and then computes the
     * SHA-256 hash of the combined input. The resulting hash is converted to a
     * hexadecimal string.
     *
     * @param password The plain text password String to hash.
     * @param saltHex The hexadecimal String representation of the salt to use.
     * @return A String containing the hexadecimal representation of the SHA-256 hash,
     *         or an empty string if the salt is invalid or memory allocation fails.
     */
    String hashPassword(const String& password, const String& saltHex) {
        unsigned char saltBytes[saltHex.length() / 2];
        size_t saltLen = hexToBytes(saltHex, saltBytes, sizeof(saltBytes));

        if (saltLen == 0) {
            Serial.println("Error: Invalid salt hex string in hashPassword.");
            return "";
        }

        // Combine password and salt: salt + password
        // Using salt first is a common practice, though order impact is debated.
        size_t combinedLen = saltLen + password.length();
        unsigned char* combinedInput = new unsigned char[combinedLen];
        if (!combinedInput) {
             Serial.println("Error: Memory allocation failed for combined input.");
             return "";
        }

        memcpy(combinedInput, saltBytes, saltLen);
        memcpy(combinedInput + saltLen, password.c_str(), password.length());

        // SHA-256 Hash
        unsigned char hashOutput[32]; // SHA-256 produces a 32-byte hash
        mbedtls_sha256_context ctx;
        mbedtls_sha256_init(&ctx);
        mbedtls_sha256_starts(&ctx, 0); // 0 for SHA-256, 1 for SHA-224
        mbedtls_sha256_update(&ctx, combinedInput, combinedLen);
        mbedtls_sha256_finish(&ctx, hashOutput);
        mbedtls_sha256_free(&ctx);

        delete[] combinedInput; // Clean up combined input buffer

        // Convert hash to hex string
        return bytesToHex(hashOutput, sizeof(hashOutput));
    }

    /**
     * @brief Verifies a plain text password against a stored hash and salt.
     *
     * Re-hashes the provided plain text password using the same salt that was used
     * to create the stored hash. Then, compares the newly calculated hash with the
     * stored hash.
     *
     * @param password The plain text password String to verify.
     * @param storedHashHex The hexadecimal String representation of the hash stored previously.
     * @param saltHex The hexadecimal String representation of the salt used with the stored hash.
     * @return True if the calculated hash matches the stored hash, false otherwise (including errors during hashing).
     */
    bool verifyPassword(const String& password, const String& storedHashHex, const String& saltHex) {
        // Hash the provided password with the same salt
        String calculatedHashHex = hashPassword(password, saltHex);

        if (calculatedHashHex.isEmpty()) {
            return false; // Error during hashing
        }

        // Compare the calculated hash with the stored hash
        // Use a constant-time comparison if possible, though String comparison here isn't strictly constant-time.
        // For embedded systems, this level might be acceptable, but be aware.
        return calculatedHashHex.equals(storedHashHex);
    }

} // namespace AuthUtils