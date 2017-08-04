let {utils: Cu, classes: Cc, interfaces: Ci } = Components;

Cu.import("resource://gre/modules/Services.jsm");
let cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);

var observer = {
  observe: function(subject, topic, data) {
    if (topic == "cookie-changed") {
      let cookie = subject.QueryInterface(Ci.nsICookie2);
      sendAsyncMessage("cookieName", cookie.name + "=" + cookie.value);
    }
  }
};

addMessageListener("createObserver" , function(e) {
  Services.obs.addObserver(observer, "cookie-changed");
  sendAsyncMessage("createObserver:return");
});

addMessageListener("removeObserver" , function(e) {
  Services.obs.removeObserver(observer, "cookie-changed");
  sendAsyncMessage("removeObserver:return");
});
