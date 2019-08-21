"use strict";

module.exports = {
  "env": {
    "webextensions": true,
  },

  globals: {
    "getTestConfig": false,
    "startMark": true,
    "endMark": true,
    "name": true,
  },

  "plugins": [
    "mozilla"
  ],

  "rules": {
    "mozilla/avoid-Date-timing": "error"
  }
};
