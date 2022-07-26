/* eslint-env mozilla/chrome-script */

"use strict";

// eslint-disable-next-line mozilla/use-services
let cs = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);

addMessageListener("getCookieCountAndClear", () => {
  let count = cs.cookies.length;
  cs.removeAll();

  sendAsyncMessage("getCookieCountAndClear:return", { count });
});

cs.removeAll();
