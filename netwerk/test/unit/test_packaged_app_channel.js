//
// This tests checks that a regular http channel can be used to load a
// packaged app resource. It sets the network.http.enable-packaged-apps pref
// to enable this feature, then creates a channel and loads the resource.
// reset_pref resets the pref at the end of the test.

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "uri", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

const nsIBinaryInputStream = Components.Constructor("@mozilla.org/binaryinputstream;1",
                              "nsIBinaryInputStream",
                              "setInputStream"
                              );


function make_channel(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newChannel2(url,
                         "",
                         null,
                         null,      // aLoadingNode
                         Services.scriptSecurityManager.getSystemPrincipal(),
                         null,      // aTriggeringPrincipal
                         Ci.nsILoadInfo.SEC_NORMAL,
                         Ci.nsIContentPolicy.TYPE_OTHER);
}

function Listener(callback) {
    this._callback = callback;
}

Listener.prototype = {
    gotStartRequest: false,
    available: -1,
    gotStopRequest: false,
    QueryInterface: function(iid) {
        if (iid.equals(Ci.nsISupports) ||
            iid.equals(Ci.nsIRequestObserver))
            return this;
        throw Cr.NS_ERROR_NO_INTERFACE;
    },
    onDataAvailable: function(request, ctx, stream, offset, count) {
        try {
            this.available = stream.available();
            do_check_eq(this.available, count);
            // Need to consume stream to avoid assertion
            var str = new nsIBinaryInputStream(stream).readBytes(count);
            equal(str, "<html>\r\n  <head>\r\n    <script src=\"/scripts/app.js\"></script>\r\n    ...\r\n  </head>\r\n  ...\r\n</html>\r\n", "check proper content");
        }
        catch (ex) {
            do_throw(ex);
        }
    },
    onStartRequest: function(request, ctx) {
        this.gotStartRequest = true;
    },
    onStopRequest: function(request, ctx, status) {
        this.gotStopRequest = true;
        do_check_eq(status, 0);
        if (this._callback) {
            this._callback.call(null, this);
        }
    }
};

// The package content
// getData formats it as described at http://www.w3.org/TR/web-packaging/#streamable-package-format
var testData = {
  content: [
   { headers: ["Content-Location: /index.html", "Content-Type: text/html"], data: "<html>\r\n  <head>\r\n    <script src=\"/scripts/app.js\"></script>\r\n    ...\r\n  </head>\r\n  ...\r\n</html>\r\n", type: "text/html" },
   { headers: ["Content-Location: /scripts/app.js", "Content-Type: text/javascript"], data: "module Math from '/scripts/helpers/math.js';\r\n...\r\n", type: "text/javascript" },
   { headers: ["Content-Location: /scripts/helpers/math.js", "Content-Type: text/javascript"], data: "export function sum(nums) { ... }\r\n...\r\n", type: "text/javascript" }
  ],
  token : "gc0pJq0M:08jU534c0p",
  getData: function() {
    var str = "";
    for (var i in this.content) {
      str += "--" + this.token + "\r\n";
      for (var j in this.content[i].headers) {
        str += this.content[i].headers[j] + "\r\n";
      }
      str += "\r\n";
      str += this.content[i].data + "\r\n";
    }

    str += "--" + this.token + "--";
    return str;
  }
}

function contentHandler(metadata, response)
{
  response.setHeader("Content-Type", 'application/package');
  var body = testData.getData();
  response.bodyOutputStream.write(body, body.length);
}

function regularContentHandler(metadata, response)
{
  var body = "response";
  response.bodyOutputStream.write(body, body.length);
}

var httpserver = null;
var originalPref = false;

function run_test()
{
  // setup test
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/package", contentHandler);
  httpserver.registerPathHandler("/regular", regularContentHandler);
  httpserver.start(-1);

  // Enable the feature and save the original pref value
  originalPref = Services.prefs.getBoolPref("network.http.enable-packaged-apps");
  Services.prefs.setBoolPref("network.http.enable-packaged-apps", true);
  do_register_cleanup(reset_pref);

  add_test(test_channel);
  add_test(test_channel_no_notificationCallbacks);
  add_test(test_channel_uris);

  // run tests
  run_next_test();
}

function test_channel(aNullNotificationCallbacks) {
  var channel = make_channel(uri+"/package!//index.html");

  if (!aNullNotificationCallbacks) {
    channel.notificationCallbacks = new LoadContextCallback(1024, false, false, false);
  }

  channel.asyncOpen(new Listener(function(l) {
    // XXX: no content length available for this resource
    //do_check_true(channel.contentLength > 0);
    do_check_true(l.gotStartRequest);
    do_check_true(l.gotStopRequest);
    run_next_test();
  }), null);
}

function test_channel_no_notificationCallbacks() {
  test_channel(true);
}

function test_channel_uris() {
  // A `!//` in the query or ref should not be handled as a packaged app resource
  var channel = make_channel(uri+"/regular?bla!//bla#bla!//bla");
  channel.asyncOpen(new ChannelListener(check_regular_response, null), null);
}

function check_regular_response(request, buffer) {
  request.QueryInterface(Components.interfaces.nsIHttpChannel);
  do_check_eq(request.responseStatus, 200);
  do_check_eq(buffer, "response");
  run_next_test();
}

function reset_pref() {
  // Set the pref to its original value
  Services.prefs.setBoolPref("network.http.enable-packaged-apps", originalPref);
}
