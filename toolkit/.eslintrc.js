"use strict";

module.exports = {
  rules: {
    // XXX Bug 1326071 - This should be reduced down - probably to 20 or to
    // be removed & synced with the mozilla/recommended value.
    "complexity": ["error", 41],

    "mozilla/no-task": "error",
  }
};
