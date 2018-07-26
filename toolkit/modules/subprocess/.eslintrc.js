"use strict";

module.exports = {
  "extends": "../../components/extensions/.eslintrc.js",

  "env": {
    "mozilla/chrome-worker": true,
  },
  "plugins": [
    "mozilla"
  ],
  "rules": {
    "no-console": "off",
  },
};
