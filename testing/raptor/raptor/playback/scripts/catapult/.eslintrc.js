"use strict";

module.exports = {

  globals: {
    "browser": [],
    "chrome": [],
    "getTestConfig": true,
    "startMark": [],
    "endMark": [],
    "name": "",
  },

  "plugins": [
    "mozilla"
  ],

  "rules": {
    "mozilla/avoid-Date-timing": 0,
    "no-global-assign":0,
    "no-undef": 0,

  }
};
