/* eslint-env mozilla/chrome-script */

"use strict";

var observer = {
  observe(subject, topic, data) {
    if (topic == "cookie-changed") {
      let cookie = subject.QueryInterface(Ci.nsICookie);
      sendAsyncMessage("cookieName", cookie.name + "=" + cookie.value);
      sendAsyncMessage("cookieOperation", data);
    }
  },
};

addMessageListener("createObserver", function(e) {
  Services.obs.addObserver(observer, "cookie-changed");
  sendAsyncMessage("createObserver:return");
});

addMessageListener("removeObserver", function(e) {
  Services.obs.removeObserver(observer, "cookie-changed");
  sendAsyncMessage("removeObserver:return");
});
