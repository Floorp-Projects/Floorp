/* eslint-env mozilla/chrome-script */

"use strict";

var observer = {
  observe(subject, topic) {
    if (topic == "cookie-changed") {
      let notification = subject.QueryInterface(Ci.nsICookieNotification);
      let cookie = notification.cookie.QueryInterface(Ci.nsICookie);
      sendAsyncMessage("cookieName", cookie.name + "=" + cookie.value);
      sendAsyncMessage("cookieOperation", notification.action);
    }
  },
};

addMessageListener("createObserver", function (e) {
  Services.obs.addObserver(observer, "cookie-changed");
  sendAsyncMessage("createObserver:return");
});

addMessageListener("removeObserver", function (e) {
  Services.obs.removeObserver(observer, "cookie-changed");
  sendAsyncMessage("removeObserver:return");
});
