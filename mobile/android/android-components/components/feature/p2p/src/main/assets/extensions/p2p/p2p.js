/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let port = browser.runtime.connectNative("mozacP2P");

port.onMessage.addListener((message) => {
    switch (message.action) {
    case 'get_html':
        inlinePics();
        captureCss(); // captureCss() will asynchronously call sendResponse().
        break;
    default:
        console.log("I do not know how to handle this action: ${message.action}")
}});

function sendResponse() {
    port.postMessage(window.document.documentElement.innerHTML);
}

function inlinePics() {
    // Based on code from https://developer.mozilla.org/en-US/docs/Web/API/HTMLCanvasElement/toDataURL
    var images = window.document.getElementsByTagName('img');
    for (var canvas, context, image, i = 0; i < images.length; i++) {
        image = images[i];
        if (image.src.toLowerCase().startsWith("data")) {
            continue;
        }
        try {
            canvas = window.document.createElement('canvas');
            context = canvas.getContext('2d');
            canvas.width = image.offsetWidth;
            canvas.height = image.offsetHeight;
            context.drawImage(image, 0, 0);
            image.src = canvas.toDataURL('image/png', .5); // compression factor
        } catch(error) {
            console.error("Error transforming " + image.src + ": " + error)
        }
    }
}

// Code from Irakli
const copyStyleSheet = (sheet) => {
    const {disabled, href, title, type} = sheet
    return {disabled, href, title, type}
}

function insertStyle(styles) {
    if (styles) {
        var css = document.createElement('style');
        css.type = "text/css";
        if (css.styleSheet) {
            css.styleSheet.cssText = styles;
        } else {
            css.appendChild(document.createTextNode(styles));
        }
        window.document.getElementsByTagName('head')[0].appendChild(css);
    }
}

// Adapted from https://stackoverflow.com/a/32596557/631051 [author: sleeplessnerd]
function captureCss() {
    var cssrules = "";
    var sheets = document.styleSheets;
    var outstandingReqs = 0;
    var head = window.document.getElementsByTagName('head')[0];
    if (!head) {
        sendResponse();
    }
    for (var i = 0; i < sheets.length; i++){
        if (!sheets[i].disabled && sheets[i].href != null) { // or sheets[i].href.nodeName == 'LINK'
            sheets[i].crossorigin = "anonymous"; // https://discourse.mozilla.org/t/webextensions-porting-access-to-cross-origin-document-stylesheets-cssrules/18359/2
            var sheet = copyStyleSheet(sheets[i]); // clone to force reevaluation per above link
            if (sheet.rules == null) { // can be null because of cross origin policy
                try {
                    var xhr = new XMLHttpRequest();
                    xhr.onreadystatechange = function() {
                        if (this.readyState == XMLHttpRequest.DONE) {
                            insertStyle(this.responseText);
                            if (--outstandingReqs == 0) {
                                sendResponse();
                                // Make negative so sendResponse() won't be called again at the
                                // end of captureCss() if it has not already been reached.
                                outstandingReqs--;
                            }
                        }};
                    xhr.open('GET', sheet.href);
                    outstandingReqs++;
                    xhr.send();
                } catch(e) {
                    console.error(e);
                }
            } else {
                for (var j = 0; j < sheet.rules.length; j++) {
                    insertStyle(sheets[i].rules[j].cssText);
                }
            }
        }
    }
    // Remove stylesheet links.
    window.document.querySelectorAll('link[rel="stylesheet"]').forEach(e => e.parentNode.removeChild(e));

    if (outstandingReqs == 0) {
        sendResponse();
    }
}

port.onDisconnect.addListener((p) => {
    if (p.error) {
        console.log("Wah! Disconnected due to an error: ${p.error.message}");
    }
});
