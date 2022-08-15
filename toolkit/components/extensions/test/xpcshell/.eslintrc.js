"use strict";

module.exports = {
  env: {
    // The tests in this folder are testing based on WebExtensions, so lets
    // just define the webextensions environment here.
    webextensions: true,
    // Many parts of WebExtensions test definitions (e.g. content scripts) also
    // interact with the browser environment, so define that here as we don't
    // have an easy way to handle per-function/scope usage yet.
    browser: true,
  },
};
