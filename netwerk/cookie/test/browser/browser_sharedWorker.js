CookiePolicyHelper.runTest("SharedWorker", {
  cookieJarAccessAllowed: async _ => {
    new content.SharedWorker("a.js", "foo");
    ok(true, "SharedWorker is allowed");
  },

  cookieJarAccessDenied: async _ => {
    try {
      new content.SharedWorker("a.js", "foo");
      ok(false, "SharedWorker cannot be used!");
    } catch (e) {
      ok(true, "SharedWorker cannot be used!");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },
});
