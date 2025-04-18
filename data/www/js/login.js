/**
 * @file login.js
 * @brief Handles the login form submission and interaction with the login API endpoint.
 *
 * Waits for the DOM to be fully loaded, then adds an event listener to the login form.
 * On submission, it prevents the default action, validates inputs, sends credentials
 * to `/api/login` using a POST request (form-urlencoded). Displays success or error
 * messages based on the server response and redirects to `/dashboard.html` on success.
 */
document.addEventListener('DOMContentLoaded', function() {
    const loginForm = document.getElementById('loginForm');
    const errorMessage = document.getElementById('errorMessage');
    const successMessage = document.getElementById('successMessage');

    if (loginForm) {
        /**
         * @brief Event listener for the login form submission.
         *
         * Prevents default form submission. Retrieves username and password.
         * Performs basic client-side validation. Sends credentials to the `/api/login`
         * endpoint via POST request with `application/x-www-form-urlencoded` content type.
         * Handles the response: displays success message and redirects to dashboard on OK status,
         * otherwise displays an error message. Catches network errors.
         * @param {Event} e - The submit event object.
         */
        loginForm.addEventListener('submit', async function(e) {
            e.preventDefault();

            const usernameInput = document.getElementById('username');
            const passwordInput = document.getElementById('password');
            const username = usernameInput.value;
            const password = passwordInput.value;

            // Hide previous messages
            if(errorMessage) errorMessage.style.display = 'none';
            if(successMessage) successMessage.style.display = 'none';

            // Basic client-side validation
            if (!username || !password) {
                if(errorMessage) {
                    errorMessage.textContent = 'Please enter both username and password.';
                    errorMessage.style.display = 'block';
                }
                return;
            }

            try {
                const response = await fetch('/api/login', {
                    method: 'POST',
                    headers: {
                        // Send as form data as the backend currently expects
                        'Content-Type': 'application/x-www-form-urlencoded',
                    },
                    // Encode data for form submission
                    body: `username=${encodeURIComponent(username)}&password=${encodeURIComponent(password)}`
                });

                if (response.ok) {
                    // Show success message
                    if(successMessage) successMessage.style.display = 'block';

                    // Redirect to dashboard after a short delay
                    setTimeout(() => {
                        window.location.href = '/dashboard.html'; // Redirect to dashboard
                    }, 1500);
                } else {
                    // Show error message from server or generic one
                     if(errorMessage) {
                        errorMessage.textContent = 'Invalid username or password. Please try again.'; // Generic error
                        errorMessage.style.display = 'block';
                    }
                    // Clear password field on error
                    if(passwordInput) passwordInput.value = '';
                }
            } catch (error) {
                console.error('Login error:', error);
                 if(errorMessage) {
                    errorMessage.textContent = 'Connection error. Please check network and try again.';
                    errorMessage.style.display = 'block';
                }
            }
        });
    }
});