"use strict";

module.exports = { // eslint-disable-line no-undef
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
