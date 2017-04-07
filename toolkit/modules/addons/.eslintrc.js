"use strict";

module.exports = {
  "extends": "../../components/extensions/.eslintrc.js",

  "globals": {
    "addEventListener": false,
    "addMessageListener": false,
    "removeEventListener": false,
    "sendAsyncMessage": false,
    "AddonManagerPermissions": false,

    "initialProcessData": true,
  },
};
