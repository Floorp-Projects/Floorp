"use strict";

module.exports = {
  "env": {
    "webextensions": true,
  },
  "globals": {
    "ExtensionAPI": true,
    // available to frameScripts
    "addMessageListener": false,
    "content": false,
    "sendAsyncMessage": false,
  }
};
