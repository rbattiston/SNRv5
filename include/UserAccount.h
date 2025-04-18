#ifndef USER_ACCOUNT_H
#define USER_ACCOUNT_H

#include <Arduino.h>

// Define user roles
/**
 * @enum UserRole
 * @brief Defines the different access levels or roles a user can have within the system.
 */
enum UserRole {
    VIEWER,  ///< Can view information but cannot make changes.
    MANAGER, ///< Can manage schedules, possibly users (depending on implementation).
    OWNER,   ///< Full administrative privileges.
    UNKNOWN  ///< Represents an invalid or uninitialized role state.
};

// Function to convert UserRole enum to String
/**
 * @brief Converts a UserRole enum value to its lowercase string representation.
 * @param role The UserRole enum value.
 * @return A String representing the role (e.g., "viewer", "manager", "owner"), or "unknown".
 */
inline String roleToString(UserRole role) { // Added inline
    switch (role) {
        case VIEWER: return "viewer";
        case MANAGER: return "manager";
        case OWNER: return "owner";
        default: return "unknown";
    }
}

// Function to convert String to UserRole enum
/**
 * @brief Converts a string representation (case-insensitive) to its corresponding UserRole enum value.
 * @param roleStr The string representation of the role.
 * @return The corresponding UserRole enum value, or UNKNOWN if the string does not match a valid role.
 */
inline UserRole stringToRole(const String& roleStr) { // Added inline
    if (roleStr.equalsIgnoreCase("viewer")) return VIEWER;
    if (roleStr.equalsIgnoreCase("manager")) return MANAGER;
    if (roleStr.equalsIgnoreCase("owner")) return OWNER;
    return UNKNOWN;
}

// Structure to hold user account data
/**
 * @struct UserAccount
 * @brief Represents the data associated with a single user account.
 *
 * Stores the username, hashed password, salt used for hashing, and the user's role.
 * Used by UserManager to load and save user data from/to persistent storage.
 */
struct UserAccount {
    String username = "";       ///< The unique username for the account.
    String hashedPassword = ""; ///< The SHA-256 hash of the user's password combined with the salt.
    String salt = "";           ///< The unique salt used when hashing this user's password.
    UserRole role = UNKNOWN;    ///< The access role assigned to this user.
    // String userId = ""; // Consider adding a persistent unique ID if needed later

    // Basic validation
    /**
     * @brief Checks if the UserAccount structure contains valid essential information.
     * @return True if username, hashedPassword, and salt are not empty, and the role is not UNKNOWN. False otherwise.
     */
    bool isValid() const {
        return !username.isEmpty() && !hashedPassword.isEmpty() && !salt.isEmpty() && role != UNKNOWN;
    }
};

#endif // USER_ACCOUNT_H