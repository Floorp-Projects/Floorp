"use strict";

module.exports = { // eslint-disable-line no-undef
  "extends": [
    "../.eslintrc.js"
  ],

  "globals": {
    "is": true,
    "isnot": true,
    "ok": true,
    "NarrateTestUtils": true,
    "content": true,
    "ContentTaskUtils": true,
    "ContentTask": true,
    "BrowserTestUtils": true,
    "gBrowser": true,
  },

  "rules": {
    "mozilla/import-headjs-globals": "warn"
  }
};
