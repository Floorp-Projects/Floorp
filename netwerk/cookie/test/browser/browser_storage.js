"use strict";

CookiePolicyHelper.runTest("SessionStorage", {
  cookieJarAccessAllowed: async w => {
    try {
      w.sessionStorage.foo = 42;
      ok(true, "SessionStorage works");
    } catch (e) {
      ok(false, "SessionStorage works");
    }
  },

  cookieJarAccessDenied: async w => {
    try {
      w.sessionStorage.foo = 42;
      ok(false, "SessionStorage doesn't work");
    } catch (e) {
      ok(true, "SessionStorage doesn't work");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },
});

CookiePolicyHelper.runTest("LocalStorage", {
  cookieJarAccessAllowed: async w => {
    try {
      w.localStorage.foo = 42;
      ok(true, "LocalStorage works");
    } catch (e) {
      ok(false, "LocalStorage works");
    }
  },

  cookieJarAccessDenied: async w => {
    try {
      w.localStorage.foo = 42;
      ok(false, "LocalStorage doesn't work");
    } catch (e) {
      ok(true, "LocalStorage doesn't work");
      is(e.name, "SecurityError", "We want a security error message.");
    }
  },
});
