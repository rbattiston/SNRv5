#include "UserManager.h"
#include "AuthUtils.h"
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h> // V7

// --- UserManager Implementation ---

/**
 * @brief Constructs a UserManager object.
 *
 * Initializes the manager with the path to the directory where user account
 * files will be stored.
 *
 * @param userDir The path within the LittleFS filesystem where user JSON files
 *                are stored (e.g., "/users").
 */
UserManager::UserManager(const char* userDir) : _userDir(userDir) {}

/**
 * @brief Constructs the full file path for a given username.
 *
 * Performs basic sanitization on the username to prevent directory traversal
 * (replaces '/', '\', '..') and appends ".json".
 *
 * @param username The username for which to get the file path.
 * @return A String containing the full path (e.g., "/users/john_doe.json"),
 *         or an empty string if the sanitized username is empty.
 */
String UserManager::getUserFilePath(const String& username) {
    // Basic sanitization: prevent directory traversal attempts
    String cleanUsername = username;
    cleanUsername.replace("/", "_"); // Replace slashes
    cleanUsername.replace("\\", "_");
    cleanUsername.replace("..", "_"); // Prevent traversal
    if (cleanUsername.isEmpty()) {
        return ""; // Invalid username
    }
    return _userDir + "/" + cleanUsername + ".json";
}

/**
 * @brief Initializes the UserManager.
 *
 * Ensures the user directory exists, creating it if necessary.
 * Checks if any user files exist in the directory. If not, it calls
 * `createDefaultOwner()` to create the initial administrative account.
 *
 * @return True if initialization is successful (directory exists, default owner
 *         created if needed), false if critical failures occur (e.g., cannot
 *         create user directory or default owner account).
 */
bool UserManager::begin() {
    Serial.println("Initializing UserManager...");
    if (!LittleFS.exists(_userDir)) {
        Serial.printf("User directory '%s' not found. Attempting to create.\n", _userDir.c_str());
        if (!LittleFS.mkdir(_userDir)) {
             Serial.printf("FATAL: Failed to create user directory '%s'. Halting.\n", _userDir.c_str());
             return false; // Cannot proceed without user directory
        }
    }

    if (!doesAnyUserExist()) {
        Serial.println("No users found. Creating default owner account.");
        if (!createDefaultOwner()) {
            Serial.println("FATAL: Failed to create default owner account.");
            return false;
        }
    } else {
        Serial.println("Existing users found.");
    }
    Serial.println("UserManager initialized successfully.");
    return true;
}


/**
 * @brief Loads user account data from a file into a UserAccount struct.
 *
 * Gets the file path for the username, opens the file, parses the JSON content,
 * and populates the provided `UserAccount` struct. Validates the loaded data
 * using `account.isValid()`.
 *
 * @param username The username of the account to load.
 * @param account A reference to a `UserAccount` struct to be populated with the loaded data.
 * @return True if the user file was found, loaded, parsed successfully, and the
 *         data is valid. False otherwise.
 */
bool UserManager::loadUser(const String& username, UserAccount& account) {
    Serial.printf("UserManager::loadUser - Entered. Checking _userDir: %s\n", _userDir.c_str()); // Check if 'this' is valid
    String filePath = getUserFilePath(username);
    if (filePath.isEmpty() || !LittleFS.exists(filePath)) {
        // Serial.printf("User file not found: %s\n", filePath.c_str()); // Debug only
        return false;
    }

    File userFile = LittleFS.open(filePath, "r");
    if (!userFile) {
        Serial.printf("Failed to open user file for reading: %s\n", filePath.c_str());
        return false;
    }

    // V7: Use JsonDocument
    JsonDocument doc;
    // StaticJsonDocument<512> doc; // Or static if size is known

    DeserializationError error = deserializeJson(doc, userFile);
    userFile.close();

    if (error) {
        Serial.printf("Failed to parse user file %s: %s\n", filePath.c_str(), error.c_str());
        return false;
    }

    Serial.println("UserManager::loadUser - Populating account struct...");
    Serial.println("UserManager::loadUser - Assigning username...");
    account.username = doc["username"] | ""; // Use default "" if key missing
    Serial.println("UserManager::loadUser - Assigning hashedPassword...");
    account.hashedPassword = doc["hashedPassword"] | "";
    Serial.println("UserManager::loadUser - Assigning salt...");
    account.salt = doc["salt"] | "";
    Serial.println("UserManager::loadUser - Assigning role...");
    account.role = stringToRole(doc["role"] | "unknown");
    Serial.println("UserManager::loadUser - Account struct populated.");

    Serial.println("UserManager::loadUser - Calling account.isValid()...");
    bool valid = account.isValid();
    Serial.printf("UserManager::loadUser - account.isValid() returned: %s\n", valid ? "true" : "false");
    if (!valid) {
         Serial.printf("Loaded user data is invalid for %s\n", username.c_str());
         return false; // Data integrity issue
    }

    Serial.printf("User loaded successfully: %s\n", username.c_str()); // Re-enabled for debugging
    return true;
}

/**
 * @brief Saves user account data from a UserAccount struct to its file.
 *
 * Validates the `UserAccount` struct first. Gets the file path for the username.
 * Opens the file in write mode (truncating existing content). Serializes the
 * `UserAccount` data into JSON format and writes it to the file.
 *
 * @param account A constant reference to the `UserAccount` struct containing the data to save.
 * @return True if the account data is valid and was saved successfully, false otherwise.
 */
bool UserManager::saveUser(const UserAccount& account) {
    if (!account.isValid()) {
        Serial.println("Attempted to save invalid user account data.");
        return false;
    }

    String filePath = getUserFilePath(account.username);
    if (filePath.isEmpty()) {
         Serial.println("Cannot save user with invalid username.");
         return false;
    }

    File userFile = LittleFS.open(filePath, "w");
    if (!userFile) {
        Serial.printf("Failed to open user file for writing: %s\n", filePath.c_str());
        return false;
    }

    // V7: Use JsonDocument
    JsonDocument doc;
    // StaticJsonDocument<512> doc; // Or static

    doc["username"] = account.username;
    doc["hashedPassword"] = account.hashedPassword;
    doc["salt"] = account.salt;
    doc["role"] = roleToString(account.role);

    if (serializeJson(doc, userFile) == 0) {
        Serial.printf("Failed to write user data to file: %s\n", filePath.c_str());
        userFile.close();
        return false;
    }

    userFile.close();
    // Serial.printf("User saved successfully: %s\n", account.username.c_str()); // Debug only
    return true;
}

/**
 * @brief Finds a user by username and loads their data.
 *
 * This is a convenience function that simply calls `loadUser`.
 *
 * @param username The username of the account to find.
 * @param account A reference to a `UserAccount` struct to be populated if the user is found.
 * @return True if the user was found and loaded successfully, false otherwise.
 */
bool UserManager::findUserByUsername(const String& username, UserAccount& account) {
    // This is essentially the same as loading the user file
    return loadUser(username, account);
}

/**
 * @brief Adds a new user account to the system.
 *
 * Checks for invalid parameters and if the username already exists.
 * Generates a salt, hashes the provided plain password, creates a new
 * `UserAccount` struct, and saves it using `saveUser`.
 *
 * @param username The username for the new account.
 * @param plainPassword The plain text password for the new account.
 * @param role The UserRole for the new account.
 * @return True if the user was added successfully, false if parameters are invalid,
 *         username exists, or saving fails.
 */
bool UserManager::addUser(const String& username, const String& plainPassword, UserRole role) {
    if (username.isEmpty() || plainPassword.isEmpty() || role == UNKNOWN) {
        Serial.println("Error: Invalid parameters for addUser.");
        return false;
    }

    String filePath = getUserFilePath(username);
    if (filePath.isEmpty()) {
        Serial.println("Error: Invalid username for addUser.");
        return false;
    }
    if (LittleFS.exists(filePath)) {
        Serial.printf("Error: User '%s' already exists.\n", username.c_str());
        return false;
    }

    UserAccount newUser;
    newUser.username = username;
    newUser.role = role;
    newUser.salt = AuthUtils::generateSalt();
    if (newUser.salt.isEmpty()) {
        Serial.println("Error: Failed to generate salt for new user.");
        return false;
    }
    newUser.hashedPassword = AuthUtils::hashPassword(plainPassword, newUser.salt);
     if (newUser.hashedPassword.isEmpty()) {
        Serial.println("Error: Failed to hash password for new user.");
        return false;
    }

    if (!newUser.isValid()) {
         Serial.println("Error: Constructed user account data is invalid before saving.");
         return false;
    }

    Serial.printf("Adding new user: %s, Role: %s\n", username.c_str(), roleToString(role).c_str());
    return saveUser(newUser);
}

/**
 * @brief Deletes a user account from the system.
 *
 * Gets the file path for the username and removes the corresponding file from LittleFS.
 * Does not currently prevent deletion of the last owner account.
 *
 * @param username The username of the account to delete.
 * @return True if the user file was found and deleted successfully, false otherwise.
 */
bool UserManager::deleteUser(const String& username) {
    String filePath = getUserFilePath(username);
     if (filePath.isEmpty() || !LittleFS.exists(filePath)) {
        Serial.printf("Error: Cannot delete non-existent user '%s'.\n", username.c_str());
        return false;
    }

    // Add check: Prevent deleting the last owner? Or handle elsewhere?
    // For now, allow deletion.

    Serial.printf("Deleting user: %s\n", username.c_str());
    if (!LittleFS.remove(filePath)) {
        Serial.printf("Error: Failed to remove user file: %s\n", filePath.c_str());
        return false;
    }
    return true;
}

/**
 * @brief Updates the password for an existing user account.
 *
 * Loads the user account, generates a new salt (important for security),
 * hashes the new plain password with the new salt, updates the `UserAccount`
 * struct, and saves it back to the file.
 *
 * @param username The username of the account to update.
 * @param newPlainPassword The new plain text password.
 * @return True if the user was found and the password updated successfully, false otherwise.
 */
bool UserManager::updateUserPassword(const String& username, const String& newPlainPassword) {
    UserAccount account;
    if (!loadUser(username, account)) {
        Serial.printf("Error: User '%s' not found for password update.\n", username.c_str());
        return false;
    }

    // Generate a new salt when changing password (good practice)
    account.salt = AuthUtils::generateSalt();
    if (account.salt.isEmpty()) {
        Serial.println("Error: Failed to generate new salt for password update.");
        return false;
    }
    account.hashedPassword = AuthUtils::hashPassword(newPlainPassword, account.salt);
     if (account.hashedPassword.isEmpty()) {
        Serial.println("Error: Failed to hash new password for update.");
        return false;
    }

    Serial.printf("Updating password for user: %s\n", username.c_str());
    return saveUser(account);
}

/**
 * @brief Updates the role for an existing user account.
 *
 * Loads the user account, updates the role in the `UserAccount` struct,
 * and saves it back to the file. Prevents updating the role to UNKNOWN.
 * Does not currently prevent changing the role of the last owner account.
 *
 * @param username The username of the account to update.
 * @param newRole The new UserRole for the account.
 * @return True if the user was found and the role updated successfully, false otherwise.
 */
bool UserManager::updateUserRole(const String& username, UserRole newRole) {
     if (newRole == UNKNOWN) {
        Serial.println("Error: Cannot update user role to UNKNOWN.");
        return false;
    }

    UserAccount account;
    if (!loadUser(username, account)) {
        Serial.printf("Error: User '%s' not found for role update.\n", username.c_str());
        return false;
    }

    // Add check: Prevent changing the last owner's role? Or handle elsewhere?
    // For now, allow role change.

    account.role = newRole;

    Serial.printf("Updating role for user '%s' to '%s'\n", username.c_str(), roleToString(newRole).c_str());
    return saveUser(account);
}

/**
 * @brief Checks if any user account files exist in the user directory.
 *
 * Opens the user directory and iterates through its contents. Returns true
 * as soon as it finds any file ending with ".json".
 *
 * @return True if at least one potential user file (.json) exists, false otherwise
 *         or if the user directory cannot be opened.
 */
bool UserManager::doesAnyUserExist() {
    File root = LittleFS.open(_userDir);
    if (!root) {
        Serial.printf("Error opening user directory '%s' to check for users.\n", _userDir.c_str());
        return false; // Or maybe true to prevent default creation? Depends on desired safety. Assume false.
    }
    if (!root.isDirectory()) {
         Serial.printf("Error: '%s' is not a directory.\n", _userDir.c_str());
         root.close();
         return false;
    }
    File file = root.openNextFile();
    bool userFound = false;
    while (file) {
        // Simple check: does the directory contain *any* file?
        // More robust: check if it's a .json file and maybe try parsing briefly?
        // For now, any file indicates a user might exist.
        if (!file.isDirectory()) {
             // Check if filename ends with .json (basic check)
             if (String(file.name()).endsWith(".json")) {
                userFound = true;
                // Serial.printf("Found potential user file: %s\n", file.name()); // Debug
                break; // Found one, no need to check further
             }
        }
        file = root.openNextFile();
    }
    root.close();
    // Serial.printf("User existence check result: %s\n", userFound ? "true" : "false"); // Debug
    return userFound;
}


/**
 * @brief Creates the default owner account if no users exist.
 *
 * Uses predefined default credentials ("owner" / "password") and the OWNER role.
 * Calls `addUser` to create and save the account.
 * Prints warnings about the insecure default password to the Serial monitor.
 *
 * @return True if the default owner account was created successfully, false otherwise.
 */
bool UserManager::createDefaultOwner() {
    // IMPORTANT: Change this default password immediately after first login!
    String defaultUsername = "owner";
    String defaultPassword = "password"; // VERY INSECURE DEFAULT!
    Serial.println("***********************************************************");
    Serial.printf("Creating default owner account: '%s'\n", defaultUsername.c_str());
    Serial.printf("DEFAULT PASSWORD: '%s'\n", defaultPassword.c_str());
    Serial.println("!!! CHANGE THIS PASSWORD IMMEDIATELY VIA THE WEB INTERFACE !!!");
    Serial.println("***********************************************************");
    return addUser(defaultUsername, defaultPassword, OWNER);
}