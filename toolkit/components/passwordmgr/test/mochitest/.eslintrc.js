"use strict";

module.exports = {
  globals: {
    promptDone: true,
    startTest: true,
    // Make no-undef happy with our runInParent mixed environments since you
    // can't indicate a single function is a new env.
    assert: true,
    addMessageListener: true,
    sendAsyncMessage: true,
    Assert: true,
  },
  rules: {
    "no-var": "off",
  },
};
