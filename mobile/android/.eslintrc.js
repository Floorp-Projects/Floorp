"use strict";

module.exports = {
  globals: {
    // TODO: Create custom rule for `Cu.import`
    "AddonManager": false,
    "AppConstants": false,
    "Downloads": false,
    "File": false,
    "FileUtils": false,
    "HelperApps": true, // TODO: Can be more specific here.
    "JNI": true, // TODO: Can be more specific here.
    "LightweightThemeManager": false,
    "Messaging": false,
    "Notifications": false,
    "OS": false,
    "ParentalControls": false,
    "PrivateBrowsingUtils": false,
    "Prompt": false,
    "Services": false,
    "SharedPreferences": false,
    "strings": false,
    "Strings": false,
    "Task": false,
    "TelemetryStopwatch": false,
    "UITelemetry": false,
    "UserAgentOverrides": 0,
    "XPCOMUtils": false,
    "ctypes": false,
    "exports": false,
    "importScripts": false,
    "module": false,
    "require": false,
    "uuidgen": false,

    "Iterator": false // TODO: Remove - deprecated!
  },
  rules: {
    // XXX Bug 1358949 - This should be reduced down - probably to 20 or to
    // be removed & synced with the mozilla/recommended value.
    "complexity": ["error", 31],

    // Rules enabled in mozilla/recommended, and disabled for now, we should
    // re-enable these over time.
    "block-spacing": "off",
    "brace-style": "off",
    "comma-spacing": "off",
    "consistent-return": "off",
    "eol-last": "off",
    "key-spacing": "off",
    "keyword-spacing": "off",
    "no-else-return": "off",
    "no-empty": "off",
    "no-extra-bind": "off",
    "no-extra-semi": "off",
    "no-lonely-if": "off",
    "no-multi-spaces": "off",
    "no-native-reassign": "off",
    "no-nested-ternary": "off",
    "no-new-object": "off",
    "no-octal": "off",
    "no-redeclare": "off",
    "no-trailing-spaces": "off",
    "no-useless-call": "off",
    "no-useless-concat": "off",
    "no-useless-return": "off",
    "no-undef": "off",
    "no-unused-vars": "off",
    "object-shorthand": "off",
    "quotes": "off", // [2, "double"]
    "space-before-blocks": "off",
    "space-before-function-paren": "off",
    "space-infix-ops": "off",
    "spaced-comment": "off",
  }
};
