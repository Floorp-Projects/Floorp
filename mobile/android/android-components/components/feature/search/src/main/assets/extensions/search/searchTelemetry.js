/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Send
 * - current URL
 * - cookies of this page
 * to the native application.
 */
function sendCurrentState() {
    let message = {
     'url': document.location.href,
     'cookies': getCookies()
    };
    browser.runtime.sendNativeMessage("MozacBrowserSearchMessage", message);
}

/**
 * Get all cookies for the current document.
 *
 * @return {Array<{name: string, value: string}>} containing all cookies.
 */
function getCookies() {
    let cookiesList = document.cookie.split("; ");
    let result = [];

    cookiesList.forEach(cookie => {
        var [name, ...value] = cookie.split('=');
        // For that special cases where the cookie value contains '='.
        value = value.join("=");

        result.push({
            "name" : name,
            "value" : value
        });
    });

    return result;
}

// Whenever a page is first accessed or when loaded from cache
// send all needed data about the search provider to the app.
const events = ["pageshow", "load"];
const eventLogger = event => {
    switch (event.type) {
    case "load":
        sendCurrentState();
        break;
    case "pageshow":
        if (event.persisted) {
            sendCurrentState();
        }
        break;
    default:
        console.log('Event:', event.type);
    }
};
events.forEach(eventName =>
    window.addEventListener(eventName, eventLogger)
);
