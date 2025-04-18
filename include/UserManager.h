#ifndef USER_MANAGER_H
#define USER_MANAGER_H

#include <Arduino.h>
#include "UserAccount.h" // Include the UserAccount structure definition
#include <vector>       // To potentially return lists of users

/**
 * @class UserManager
 * @brief Manages user accounts stored in individual files within a directory.
 *
 * Handles loading, saving, adding, deleting, and updating user account information.
 * Each user account is stored as a separate JSON file in the specified directory
 * (default: "/users"). Includes functionality to create a default owner account
 * on first initialization if no users exist.
 */
class UserManager {
public:
    /**
     * @brief Constructs a UserManager object.
     * @param userDir The path within LittleFS to the directory where user account files are stored. Defaults to "/users".
     */
    UserManager(const char* userDir = "/users");

    // Initializes the UserManager, potentially creating a default owner if none exist.
    /**
     * @brief Initializes the UserManager.
     *
     * Ensures the user directory exists. If no user files are found, it creates
     * a default owner account with predefined credentials.
     * @return True if initialization is successful, false on critical failure (e.g., cannot create directory or default user).
     */
    bool begin();

    // Loads a user account from its file (e.g., /users/username.json)
    // Returns true and populates 'account' if successful, false otherwise.
    /**
     * @brief Loads a user account from its file (e.g., /users/username.json).
     * @param username The username of the account to load.
     * @param account Reference to a UserAccount struct to be populated with the loaded data.
     * @return True if the user file was found, loaded, and parsed successfully, false otherwise.
     */
    bool loadUser(const String& username, UserAccount& account);

    // Saves a user account to its file (e.g., /users/username.json)
    // Returns true if successful, false otherwise.
    /**
     * @brief Saves a user account struct to its corresponding file (e.g., /users/username.json).
     *
     * Overwrites the file if it exists, creates it otherwise. Validates the account data before saving.
     * @param account Constant reference to the UserAccount struct containing the data to save.
     * @return True if the account data is valid and saved successfully, false otherwise.
     */
    bool saveUser(const UserAccount& account);

    // Finds a user by username and populates the 'account' structure.
    // Returns true if found, false otherwise.
    // (Essentially a wrapper around loadUser for finding)
    /**
     * @brief Finds a user by username and populates the 'account' structure.
     *
     * This is primarily a convenience wrapper around `loadUser`.
     * @param username The username of the account to find.
     * @param account Reference to a UserAccount struct to be populated if the user is found.
     * @return True if the user was found and loaded successfully, false otherwise.
     */
    bool findUserByUsername(const String& username, UserAccount& account);

    // Adds a new user. Hashes the password before saving.
    // Checks for existing username.
    // Returns true on success, false if user exists or save fails.
    /**
     * @brief Adds a new user account to the system.
     *
     * Hashes the provided plain password using a newly generated salt before saving.
     * Checks if a user with the same username already exists.
     * @param username The username for the new account.
     * @param plainPassword The plain text password for the new account.
     * @param role The UserRole for the new account.
     * @return True on successful addition, false if the username already exists, parameters are invalid, or saving fails.
     */
    bool addUser(const String& username, const String& plainPassword, UserRole role);

    // Deletes a user file.
    // Returns true on success, false if file doesn't exist or delete fails.
    /**
     * @brief Deletes a user account file from the filesystem.
     * @param username The username of the account to delete.
     * @return True on successful deletion, false if the user file doesn't exist or deletion fails.
     */
    bool deleteUser(const String& username);

    // Updates an existing user's password. Hashes the new password.
    // Returns true on success, false if user not found or save fails.
    /**
     * @brief Updates the password for an existing user account.
     *
     * Generates a new salt and hashes the new password before saving the updated account.
     * @param username The username of the account to update.
     * @param newPlainPassword The new plain text password.
     * @return True on successful update, false if the user is not found or saving fails.
     */
    bool updateUserPassword(const String& username, const String& newPlainPassword);

    // Updates an existing user's role.
    // Returns true on success, false if user not found or save fails.
    /**
     * @brief Updates the role for an existing user account.
     * @param username The username of the account to update.
     * @param newRole The new UserRole for the account. Cannot be UNKNOWN.
     * @return True on successful update, false if the user is not found, the new role is invalid, or saving fails.
     */
    bool updateUserRole(const String& username, UserRole newRole);

    // Checks if any user exists in the user directory.
    /**
     * @brief Checks if any user account files exist in the designated user directory.
     * @return True if at least one `.json` file is found in the user directory, false otherwise.
     */
    bool doesAnyUserExist();

    // (Optional) Get a list of all usernames
    // std::vector<String> listUsernames();

private:
    String _userDir; ///< Directory path where user account JSON files are stored.

    // Helper to get the full path for a user file
    /**
     * @brief Internal helper to construct the full file path for a user account file.
     *        Includes basic sanitization of the username.
     * @param username The username.
     * @return The full path string (e.g., "/users/username.json") or an empty string if username is invalid.
     */
    String getUserFilePath(const String& username);

    // Creates a default owner account if no users exist.
    /**
     * @brief Internal helper to create a default owner account with predefined credentials.
     *        Called by `begin()` if `doesAnyUserExist()` returns false.
     * @return True if the default owner account was created successfully, false otherwise.
     */
    bool createDefaultOwner();
};

#endif // USER_MANAGER_H