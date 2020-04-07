"use strict";

CookiePolicyHelper.runTest("DOM Cache", {
  cookieJarAccessAllowed: async w => {
    await w.caches.open("wow").then(
      _ => {
        ok(true, "DOM Cache can be used!");
      },
      _ => {
        ok(false, "DOM Cache can be used!");
      }
    );
  },

  cookieJarAccessDenied: async w => {
    await w.caches.open("wow").then(
      _ => {
        ok(false, "DOM Cache cannot be used!");
      },
      _ => {
        ok(true, "DOM Cache cannot be used!");
      }
    );
  },
});
