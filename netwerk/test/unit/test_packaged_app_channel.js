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
    gotStopRequestOK: false,
    gotFileNotFound: false,
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
        this.gotStopRequestOK = (Cr.NS_OK === status);
        this.gotFileNotFound = (Cr.NS_ERROR_FILE_NOT_FOUND === status);
        if (this._callback) {
            this._callback.call(null, this);
        }
    }
};

// The package content
// getData formats it as described at http://www.w3.org/TR/web-packaging/#streamable-package-format
var testData = {
  packageHeader: "manifest-signature: dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk\r\n",
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

function contentHandlerWithSignature(metadata, response)
{
  response.setHeader("Content-Type", 'application/package');
  var body = testData.packageHeader + testData.getData();
  response.bodyOutputStream.write(body, body.length);
}

var httpserver = null;
var originalPref = false;
var originalDevMode = false;

function run_test()
{
  // setup test
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/package", contentHandler);
  httpserver.registerPathHandler("/regular", regularContentHandler);
  httpserver.registerPathHandler("/package_with_signature", contentHandlerWithSignature);
  httpserver.start(-1);

  // Enable the feature and save the original pref value
  originalPref = Services.prefs.getBoolPref("network.http.enable-packaged-apps");
  originalDevMode = Services.prefs.getBoolPref("network.http.packaged-apps-developer-mode");
  Services.prefs.setBoolPref("network.http.enable-packaged-apps", true);
  Services.prefs.setBoolPref("network.http.packaged-apps-developer-mode", false);
  do_register_cleanup(reset_pref);

  add_test(test_channel);
  add_test(test_channel_no_notificationCallbacks);
  add_test(test_channel_uris);

  add_test(test_channel_with_signature);
  add_test(test_channel_with_signature_dev_mode);

  // run tests
  run_next_test();
}

function test_channel_with_signature() {
  var channel = make_channel(uri+"/package_with_signature!//index.html");
  channel.notificationCallbacks = new LoadContextCallback(1024, false, false, false);
  channel.asyncOpen(new Listener(function(l) {
    // Since the manifest verification is not implemented yet, we should
    // get NS_ERROR_FILE_NOT_FOUND if the package has a signature while
    // not in developer mode.
    do_check_true(l.gotFileNotFound);
    run_next_test();
  }), null);
}

function test_channel_with_signature_dev_mode() {
  Services.prefs.setBoolPref("network.http.packaged-apps-developer-mode", true);
  var channel = make_channel(uri+"/package_with_signature!//index.html");
  channel.notificationCallbacks = new LoadContextCallback(1024, false, false, false);
  channel.asyncOpen(new Listener(function(l) {
    do_check_true(l.gotStopRequestOK);
    Services.prefs.setBoolPref("network.http.packaged-apps-developer-mode", false);
    run_next_test();
  }), null);
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
    do_check_true(l.gotStopRequestOK);
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
  Services.prefs.setBoolPref("network.http.packaged-apps-developer-mode", originalDevMode);
}
