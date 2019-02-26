CookiePolicyHelper.runTest("DOM Cache", {
  cookieJarAccessAllowed: async _ => {
    await content.caches.open("wow").then(
      _ => { ok(true, "DOM Cache can be used!"); },
      _ => { ok(false, "DOM Cache can be used!"); });
  },

  cookieJarAccessDenied: async _ => {
    await content.caches.open("wow").then(
      _ => { ok(false, "DOM Cache cannot be used!"); },
      _ => { ok(true, "DOM Cache cannot be used!"); });
  },
});
