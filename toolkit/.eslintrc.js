"use strict";

module.exports = {
  rules: {
    // XXX Bug 1326071 - This should be reduced down - probably to 20 or to
    // be removed & synced with the mozilla/recommended value.
    "complexity": ["error", 41],

    "mozilla/no-task": "error",

    "mozilla/use-services": "error",
  },

  "overrides": [{
    // Turn off use-services for xml files. XBL bindings are going away, and
    // working out the valid globals for those is difficult.
    "files": "**/*.xml",
    "rules": {
      "mozilla/use-services": "off",
    }
  }]
};
