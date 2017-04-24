"use strict";

module.exports = {
  "env": {
    "mozilla/browser-window": true,
  },

  "plugins": [
    "mozilla"
  ],

  rules: {
    // XXX Bug 1358949 - This should be reduced down - probably to 20 or to
    // be removed & synced with the mozilla/recommended value.
    "complexity": ["error", 48],
  }
};
