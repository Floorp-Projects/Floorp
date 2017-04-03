"use strict";

module.exports = {
  "extends": "../../components/extensions/.eslintrc.js",

  "env": {
    "worker": true,
  },

  "globals": {
    "ChromeWorker": false,
    "Components": false,
    "LIBC": true,
    "Library": true,
    "OS": false,
    "Services": false,
    "SubprocessConstants": true,
    "ctypes": false,
    "debug": true,
    "dump": false,
    "libc": true,
    "unix": true,
  },

  "rules": {
    "no-console": "off",
  },
};
