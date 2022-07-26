/* eslint-env mozilla/chrome-script */

"use strict";

function getCookieService() {
  // eslint-disable-next-line mozilla/use-services
  return Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
}

function getCookies(cs) {
  let cookies = [];
  for (let cookie of cs.cookies) {
    cookies.push({
      host: cookie.host,
      path: cookie.path,
      name: cookie.name,
      value: cookie.value,
      expires: cookie.expires,
    });
  }
  return cookies;
}

function removeAllCookies(cs) {
  cs.removeAll();
}

addMessageListener("init", _ => {
  let cs = getCookieService();
  removeAllCookies(cs);
  sendAsyncMessage("init:return");
});

addMessageListener("getCookies", _ => {
  let cs = getCookieService();
  let cookies = getCookies(cs);
  removeAllCookies(cs);
  sendAsyncMessage("getCookies:return", { cookies });
});

addMessageListener("shutdown", _ => {
  let cs = getCookieService();
  removeAllCookies(cs);
  sendAsyncMessage("shutdown:return");
});
