// This file tests bug 250375

function check_request_header(chan, name, value) {
  var chanValue;
  try {
    chanValue = chan.getRequestHeader(name);
  } catch (e) {
    do_throw("Expected to find header '" + name + "' but didn't find it");
  }
  dump("Value for header '" + name + "' is '" + chanValue + "'\n");
  do_check_eq(chanValue, value);
}

var cookieVal = "C1=V1";

var listener = {
  onStartRequest: function test_onStartR(request, ctx) {
    try {
      var chan = request.QueryInterface(Components.interfaces.nsIChannel);
      check_request_header(chan, "Cookie", cookieVal);
    } catch (e) {
      do_throw("Unexpected exception: " + e);
    }

    throw Components.results.NS_ERROR_ABORT;
  },

  onDataAvailable: function test_ODA() {
    throw Components.results.NS_ERROR_UNEXPECTED;
  },

  onStopRequest: function test_onStopR(request, ctx, status) {
    if (this._iteration == 1 && Components.isSuccessCode(status))
      run_test_continued();
    do_test_finished();
  },

  _iteration: 1
};

function makeChan() {
  var ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
  var chan = ios.newChannel("http://www.mozilla.org/", null, null)
                .QueryInterface(Components.interfaces.nsIHttpChannel);

  return chan;
}

function run_test() {
  dump("Note: This test needs a network connection\n");

  var chan = makeChan();

  chan.setRequestHeader("Cookie", cookieVal, false);

  chan.asyncOpen(listener, null);

  do_test_pending();
}

function run_test_continued() {
  var chan = makeChan();

  var cookServ = Components.classes["@mozilla.org/cookieService;1"]
                           .getService(Components.interfaces.nsICookieService);
  var cookie2 = "C2=V2";
  cookServ.setCookieString(chan.URI, null, cookie2, chan);
  chan.setRequestHeader("Cookie", cookieVal, false);

  // We expect that the setRequestHeader overrides the
  // automatically-added one, so insert cookie2 in front
  cookieVal = cookie2 + "; " + cookieVal;

  listener._iteration++;
  chan.asyncOpen(listener, null);

  do_test_pending();
}

