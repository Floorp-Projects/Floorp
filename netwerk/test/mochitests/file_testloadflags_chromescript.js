var gObs;

function info(s) {
  sendAsyncMessage("info", { str: String(s) });
}

function ok(c, m) {
  sendAsyncMessage("ok", { c, m });
}

function is(a, b, m) {
  ok(Object.is(a, b), m + " (" + a + " === " + b + ")");
}

// Count headers.
function obs() {
  info("adding observer");

  this.os = Cc["@mozilla.org/observer-service;1"]
              .getService(Ci.nsIObserverService);
  this.os.addObserver(this, "http-on-modify-request");
}

obs.prototype = {
  observe(theSubject, theTopic, theData) {
    info("theSubject " + theSubject);
    info("theTopic " + theTopic);
    info("theData " + theData);

    var channel = theSubject.QueryInterface(Ci.nsIHttpChannel);
    info("channel " + channel);
    try {
      info("channel.URI " + channel.URI);
      info("channel.URI.spec " + channel.URI.spec);
      channel.visitRequestHeaders({
        visitHeader: function(aHeader, aValue) {
          info(aHeader + ": " + aValue);
        }});
    } catch (err) {
      ok(false, "catch error " + err);
    }

    // Ignore notifications we don't care about (like favicons)
    if (!channel.URI.spec.includes(
          "http://example.org/tests/netwerk/test/mochitests/")) {
      info("ignoring this one");
      return;
    }

    sendAsyncMessage("observer:gotCookie",
                     { cookie: channel.getRequestHeader("Cookie"),
                       uri: channel.URI.spec });
  },

  remove() {
    info("removing observer");

    this.os.removeObserver(this, "http-on-modify-request");
    this.os = null;
  }
}

function getCookieCount(cs) {
  let count = 0;
  for (let cookie of cs.enumerator) {
    info("cookie: " + cookie);
    info("cookie host " + cookie.host + " path " + cookie.path + " name " + cookie.name +
         " value " + cookie.value + " isSecure " + cookie.isSecure + " expires " + cookie.expires);
    ++count;
  }

  return count;
}

addMessageListener("init", ({ domain }) => {
  let cs = Cc["@mozilla.org/cookiemanager;1"]
             .getService(Ci.nsICookieManager);

  info("we are going to remove these cookies");

  let count = getCookieCount(cs);
  info(count + " cookies");

  cs.removeAll();
  cs.add(domain, "", "oh", "hai", false, false, true, Math.pow(2, 62), {},
         Ci.nsICookie2.SAMESITE_UNSET);
  is(cs.countCookiesFromHost(domain), 1, "number of cookies for domain " + domain);

  gObs = new obs();
  sendAsyncMessage("init:return");
});

addMessageListener("getCookieCount", () => {
  let cs = Cc["@mozilla.org/cookiemanager;1"]
             .getService(Ci.nsICookieManager);
  let count = getCookieCount(cs);

  cs.removeAll();
  sendAsyncMessage("getCookieCount:return", { count });
});

addMessageListener("shutdown", () => {
  gObs.remove();

  let cs = Cc["@mozilla.org/cookiemanager;1"]
             .getService(Ci.nsICookieManager);
  cs.removeAll();
  sendAsyncMessage("shutdown:return");
});
