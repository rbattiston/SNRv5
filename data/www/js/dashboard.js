/**
 * @file dashboard.js
 * @brief Handles fetching and displaying user information on the dashboard, and manages the logout process.
 *
 * Waits for the DOM to be fully loaded, then fetches user session data from the `/api/user` endpoint.
 * If successful, it populates the username and role elements on the page.
 * If the user is not authenticated (401 status), it redirects to the login page.
 * It also adds an event listener to the logout button to send a POST request to `/api/logout`
 * and redirects to the login page upon successful logout.
 */
document.addEventListener('DOMContentLoaded', function() {

    const usernameElement = document.getElementById('username');
    const userRoleElement = document.getElementById('userRole');
    const logoutButton = document.getElementById('logoutBtn');

    // Fetch user session data
    /**
     * @function getUserInfo
     * @brief Fetches user session information from the server API.
     *
     * Sends a GET request to `/api/user`. If successful (status 200), parses the JSON
     * response and updates the `usernameElement` and `userRoleElement` with the received data.
     * If the response status is 401 (Unauthorized), redirects the user to the login page (`/index.html`).
     * Logs errors for other non-OK statuses or network issues.
     * @async
     */
    async function getUserInfo() {
        try {
            const response = await fetch('/api/user'); // Use the user info API endpoint
            if (response.ok) {
                const userData = await response.json();
                if(usernameElement) usernameElement.textContent = userData.username || 'N/A';
                if(userRoleElement) userRoleElement.textContent = userData.role || 'N/A';
            } else if (response.status === 401) {
                // If not authenticated, redirect to login
                console.log('User not authenticated, redirecting to login.');
                window.location.href = '/index.html';
            } else {
                 console.error('Error fetching user data, status:', response.status);
                 if(usernameElement) usernameElement.textContent = 'Error';
                 if(userRoleElement) userRoleElement.textContent = 'Error';
                 // Optionally redirect to login on other errors too
                 // window.location.href = '/index.html';
            }
        } catch (error) {
            console.error('Network error fetching user data:', error);
             if(usernameElement) usernameElement.textContent = 'Error';
             if(userRoleElement) userRoleElement.textContent = 'Error';
            // Redirect to login on network error
            // window.location.href = '/index.html';
        }
    }

    // Handle logout
    if (logoutButton) {
        /**
         * @brief Event listener for the logout button click.
         *
         * Sends a POST request to the `/api/logout` endpoint.
         * If the logout is successful (status 200 OK), redirects the user to the login page (`/index.html`).
         * If the logout fails, logs an error and shows an alert to the user.
         * @param {Event} event - The click event object.
         */
        logoutButton.addEventListener('click', async function() {
            try {
                const response = await fetch('/api/logout', {
                    method: 'POST'
                    // No body needed for logout
                });

                if (response.ok) {
                    console.log('Logout successful, redirecting.');
                    window.location.href = '/index.html'; // Redirect to login page
                } else {
                    console.error('Logout failed, status:', response.status);
                    alert('Logout failed. Please try again.'); // Simple feedback
                }
            } catch (error) {
                console.error('Logout network error:', error);
                alert('Logout failed due to a network error.');
            }
        });
    }

    // Initial fetch of user info when the page loads
    getUserInfo();

});