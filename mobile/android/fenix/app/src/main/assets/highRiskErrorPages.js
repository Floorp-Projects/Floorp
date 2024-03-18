/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Handles the parsing of the ErrorPages URI and then passes them to injectValues
 */
function parseQuery(queryString) {
    if (queryString[0] === '?') {
        queryString = queryString.substr(1);
    }
    const query = Object.fromEntries(new URLSearchParams(queryString).entries());
    injectValues(query);
};

/**
 * Updates the HTML elements based on the queryMap
 */
function injectValues(queryMap) {
    // Go through each element and inject the values
    document.title = queryMap.title;
    document.getElementById('errorTitleText').innerHTML = queryMap.title;
    document.getElementById('errorShortDesc').innerHTML = queryMap.description;

    // If no image is passed in, remove the element so as not to leave an empty iframe
    const errorImage = document.getElementById('errorImage');
    if (!queryMap.image) {
        errorImage.remove();
    } else  {
        errorImage.src = "resource://android/assets/" + queryMap.image;
    }
}

document.addEventListener('DOMContentLoaded', function () {
    if (window.history.length == 1) {
        document.getElementById('backButton').style.display = 'none';
    } else {
        document.getElementById('backButton').addEventListener('click', () => window.history.back() );
    }
});

parseQuery(document.documentURI);
