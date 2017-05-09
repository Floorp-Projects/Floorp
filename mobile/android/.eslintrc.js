"use strict";

module.exports = {
  env: {
    "browser": true
  },
  globals: {
    "Components": false,

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
    "dump": false,
    "exports": false,
    "importScripts": false,
    "module": false,
    "require": false,
    "uuidgen": false,

    "Iterator": false // TODO: Remove - deprecated!
  },
  rules: {
    "global-strict": "off", // Overridden by "strict"
    "no-underscore-dangle": "off", // We allow trailing underscores in names.

    // We disable everything to get all files to pass w/o updating them.
    // We'll re-enable one by one.
    "camelcase": "off",
    "comma-dangle": "off",
    "comma-spacing": "off",

    // XXX Bug 1358949 - This should be reduced down - probably to 20 or to
    // be removed & synced with the mozilla/recommended value.
    "complexity": ["error", 31],

    "consistent-return": "off",
    "curly": "off",
    "dot-notation": "off",
    "eqeqeq": "off",
    "key-spacing": "off",
    "new-cap": "off",
    "no-caller": "off",
    "no-constant-condition": "off",
    "no-empty": "off",
    "no-extra-bind": "off",
    "no-extra-semi": "off",
    "no-loop-func": "off",
    "no-multi-spaces": "off",
    "no-new-object": "off",
    "no-octal": "off",
    "no-return-assign": "off",
    "no-shadow": "off",
    "no-trailing-spaces": "off",
    "no-unused-vars": "off",
    "no-use-before-define": "off",
    "quotes": "off", // [2, "double"]
    "semi": "off",
    "space-infix-ops": "off",
    "space-unary-ops": "off", // 2: https://github.com/eslint/eslint/issues/2764
    "strict": "off"
  }

  // "ecmaFeatures": {
  //   "forOf": true,
  //   "jsx": true,
  // },
  // "rules": {
  //   // turn off all kinds of stuff that we actually do want, because
  //   // right now, we're bootstrapping the linting infrastructure.  We'll
  //   // want to audit these rules, and start turning them on and fixing the
  //   // problems they find, one at a time.
  //
  //   // Eslint built-in rules are documented at <http://eslint.org/docs/rules/>
  //   "camelcase": 0,               // TODO: Remove (use default)
  //   "consistent-return": 0,       // TODO: Remove (use default)
  //   dot-location: 0,              // [2, property],
  //   "eqeqeq": 0,                  // TBD. Might need to be separate for content & chrome
  //   "global-strict": 0,           // Leave as zero (this will be unsupported in eslint 1.0.0)
  //   "linebreak-style": [2, "unix"],
  //   "new-cap": 0,                 // TODO: Remove (use default)
  //   "no-catch-shadow": 0,         // TODO: Remove (use default)
  //   "no-console": 0,              // Leave as 0. We use console logging in content code.
  //   "no-empty": 0,                // TODO: Remove (use default)
  //   "no-extra-bind": 0,           // Leave as 0
  //   "no-extra-boolean-cast": 0,   // TODO: Remove (use default)
  //   "no-multi-spaces": 0,         // TBD.
  //   "no-new": 0,                  // TODO: Remove (use default)
  //   "no-redeclare": 0,            // TODO: Remove (use default)
  //   "no-return-assign": 0,        // TODO: Remove (use default)
  //   "no-underscore-dangle": 0,    // Leave as 0. Commonly used for private variables.
  //   "no-unneeded-ternary": 2,
  //   "no-unused-expressions": 0,   // TODO: Remove (use default)
  //   "no-unused-vars": 0,          // TODO: Remove (use default)
  //   "no-use-before-define": 0,    // TODO: Remove (use default)
  //   "quotes": [2, "double", "avoid-escape"],
  //   "strict": 0,                  // [2, "function"],
  // }
};
