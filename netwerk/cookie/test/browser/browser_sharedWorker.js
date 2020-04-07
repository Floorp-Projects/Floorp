"use strict";

CookiePolicyHelper.runTest("SharedWorker", {
  cookieJarAccessAllowed: async w => {
    new w.SharedWorker("a.js", "foo");
    ok(true, "SharedWorker is allowed");
  },

  cookieJarAccessDenied: async w => {
    try {
      new w.SharedWorker("a.js", "foo");
      ok(false, "SharedWorker cannot be used!");
    } catch (e) {
      ok(true, "SharedWorker cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },
});
