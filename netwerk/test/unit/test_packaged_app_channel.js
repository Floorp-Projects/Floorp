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
            equal(str, "<html>\n  Last updated: 2015/10/01 14:10 PST\n</html>\n", "check proper content");
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
   { headers: ["Content-Location: /index.html", "Content-Type: text/html"], data: "<html>\n  Last updated: 2015/10/01 14:10 PST\n</html>\n", type: "text/html" },
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

var badSignature = "manifest-signature: dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk\r\n"; 
var goodSignature = "manifest-signature: MIIF1AYJKoZIhvcNAQcCoIIFxTCCBcECAQExCzAJBgUrDgMCGgUAMAsGCSqGSIb3DQEHAaCCA54wggOaMIICgqADAgECAgECMA0GCSqGSIb3DQEBCwUAMHMxCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJDQTEWMBQGA1UEBxMNTW91bnRhaW4gVmlldzEkMCIGA1UEChMbRXhhbXBsZSBUcnVzdGVkIENvcnBvcmF0aW9uMRkwFwYDVQQDExBUcnVzdGVkIFZhbGlkIENBMB4XDTE1MDkxMDA4MDQzNVoXDTM1MDkxMDA4MDQzNVowdDELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MSQwIgYDVQQKExtFeGFtcGxlIFRydXN0ZWQgQ29ycG9yYXRpb24xGjAYBgNVBAMTEVRydXN0ZWQgQ29ycCBDZXJ0MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAts8whjOzEbn/w1xkFJ67af7F/JPujBK91oyJekh2schIMzFau9pY8S1AiJQoJCulOJCJfUc8hBLKBZiGAkii+4Gpx6cVqMLe6C22MdD806Soxn8Dg4dQqbIvPuI4eeVKu5CEk80PW/BaFMmRvRHO62C7PILuH6yZeGHC4P7dTKpsk4CLxh/jRGXLC8jV2BCW0X+3BMbHBg53NoI9s1Gs7KGYnfOHbBP5wEFAa00RjHnubUaCdEBlC8Kl4X7p0S4RGb3rsB08wgFe9EmSZHIgcIm+SuVo7N4qqbI85qo2ulU6J8NN7ZtgMPHzrMhzgAgf/KnqPqwDIxnNmRNJmHTUYwIDAQABozgwNjAMBgNVHRMBAf8EAjAAMBYGA1UdJQEB/wQMMAoGCCsGAQUFBwMDMA4GA1UdDwEB/wQEAwIHgDANBgkqhkiG9w0BAQsFAAOCAQEAukH6cJUUj5faa8CuPCqrEa0PoLY4SYNnff9NI+TTAHkB9l+kOcFl5eo2EQOcWmZKYi7QLlWC4jy/KQYattO9FMaxiOQL4FAc6ZIbNyfwWBzZWyr5syYJTTTnkLq8A9pCKarN49+FqhJseycU+8EhJEJyP5pv5hLvDNTTHOQ6SXhASsiX8cjo3AY4bxA5pWeXuTZ459qDxOnQd+GrOe4dIeqflk0hA2xYKe3SfF+QlK8EO370B8Dj8RX230OATM1E3OtYyALe34KW3wM9Qm9rb0eViDnVyDiCWkhhQnw5yPg/XQfloug2itRYuCnfUoRt8xfeHgwz2Ymz8cUADn3KpTGCAf4wggH6AgEBMHgwczELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MSQwIgYDVQQKExtFeGFtcGxlIFRydXN0ZWQgQ29ycG9yYXRpb24xGTAXBgNVBAMTEFRydXN0ZWQgVmFsaWQgQ0ECAQIwCQYFKw4DAhoFAKBdMBgGCSqGSIb3DQEJAzELBgkqhkiG9w0BBwEwHAYJKoZIhvcNAQkFMQ8XDTE1MTAwMTIxMTEwNlowIwYJKoZIhvcNAQkEMRYEFHAisUYrrt+gBxYFhZ5KQQusOmN3MA0GCSqGSIb3DQEBAQUABIIBACHW4V0BsPWOvWrGOTRj6mPpNbH/JI1bN2oyqQZrpUQoaBY+BbYxO7TY4Uwe+aeIR/TTPJznOMF/dl3Bna6TPabezU4ylg7TVFI6W7zC5f5DZKp+Xv6uTX6knUzbbW1fkJqMtE8hGUzYXc3/C++Ci6kuOzrpWOhk6DpJHeUO/ioV56H0+QK/oMAjYpEsOohaPqvTPNOBhMQ0OQP3bmuJ6HcjZ/oz96PpzXUPKT1tDe6VykIYkV5NvtC8Tu2lDbYvp9ug3gyDgdyNSV47y5i/iWkzEhsAJB+9Z50wKhplnkxxVHEXkB/6tmfvExvQ28gLd/VbaEGDX2ljCaTSUjhD0o0=\r\n";

var packageContent = [
  "--7B0MKBI3UH\r",
  "Content-Location: manifest.webapp\r",
  "Content-Type: application/x-web-app-manifest+json\r",
  "\r",
  "{",
  "  \"name\": \"My App\",",
  "  \"moz-resources\": [",
  "    {",
  "      \"src\": \"page2.html\",",
  "      \"integrity\": \"JREF3JbXGvZ+I1KHtoz3f46ZkeIPrvXtG4VyFQrJ7II=\"",
  "    },",
  "    {",
  "      \"src\": \"index.html\",",
  "      \"integrity\": \"zEubR310nePwd30NThIuoCxKJdnz7Mf5z+dZHUbH1SE=\"",
  "    },",
  "    {",
  "      \"src\": \"scripts/script.js\",",
  "      \"integrity\": \"6TqtNArQKrrsXEQWu3D9ZD8xvDRIkhyV6zVdTcmsT5Q=\"",
  "    },",
  "    {",
  "      \"src\": \"scripts/library.js\",",
  "      \"integrity\": \"TN2ByXZiaBiBCvS4MeZ02UyNi44vED+KjdjLInUl4o8=\"",
  "    }",
  "  ],",
  "  \"moz-permissions\": [",
  "    {",
  "      \"systemXHR\": {",
  "        \"description\": \"Needed to download stuff\"",
  "      },",
  "      \"devicestorage:pictures\": {",
  "        \"description\": \"Need to load pictures\"",
  "      }",
  "    }",
  "  ],",
  "  \"package-identifier\": \"611FC2FE-491D-4A47-B3B3-43FBDF6F404F\",",
  "  \"moz-package-location\": \"https://example.com/myapp/app.pak\",",
  "  \"description\": \"A great app!\"",
  "}\r",
  "--7B0MKBI3UH\r",
  "Content-Location: page2.html\r",
  "Content-Type: text/html\r",
  "\r",
  "<html>",
  "  page2.html",
  "</html>",
  "\r",
  "--7B0MKBI3UH\r",
  "Content-Location: index.html\r",
  "Content-Type: text/html\r",
  "\r",
  "<html>",
  "  Last updated: 2015/10/01 14:10 PST",
  "</html>",
  "\r",
  "--7B0MKBI3UH\r",
  "Content-Location: scripts/script.js\r",
  "Content-Type: text/javascript\r",
  "\r",
  "// script.js",
  "\r",
  "--7B0MKBI3UH\r",
  "Content-Location: scripts/library.js\r",
  "Content-Type: text/javascript\r",
  "\r",
  "// library.js",
  "\r",
  "--7B0MKBI3UH--"
].join("\n");

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

function contentHandlerWithBadSignature(metadata, response)
{
  response.setHeader("Content-Type", 'application/package');
  var body = badSignature + packageContent;
  response.bodyOutputStream.write(body, body.length);
}

function contentHandlerWithGoodSignature(metadata, response)
{
  response.setHeader("Content-Type", 'application/package');
  var body = goodSignature + packageContent;
  response.bodyOutputStream.write(body, body.length);
}

var httpserver = null;
var originalPref = false;
var originalSignedAppEnabled = false;

function run_test()
{
  // setup test
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/package", contentHandler);
  httpserver.registerPathHandler("/regular", regularContentHandler);
  httpserver.registerPathHandler("/package_with_good_signature", contentHandlerWithGoodSignature);
  httpserver.registerPathHandler("/package_with_bad_signature", contentHandlerWithBadSignature);
  httpserver.start(-1);

  // Enable the feature and save the original pref value
  originalPref = Services.prefs.getBoolPref("network.http.enable-packaged-apps");
  originalSignedAppEnabled = Services.prefs.getBoolPref("network.http.packaged-signed-apps-enabled");
  Services.prefs.setBoolPref("network.http.enable-packaged-apps", true);
  Services.prefs.setBoolPref("network.http.packaged-signed-apps-enabled", true);
  do_register_cleanup(reset_pref);

  add_test(test_channel);
  add_test(test_channel_no_notificationCallbacks);
  add_test(test_channel_uris);

  add_test(test_channel_with_bad_signature);
  add_test(test_channel_with_good_signature);

  // run tests
  run_next_test();
}

function test_channel_with_bad_signature() {
  var channel = make_channel(uri+"/package_with_bad_signature!//index.html");
  channel.notificationCallbacks = new LoadContextCallback(1024, false, false, false);
  channel.asyncOpen(new Listener(function(l) {
    do_check_true(l.gotFileNotFound);
    run_next_test();
  }), null);
}

function test_channel_with_good_signature() {
  var channel = make_channel(uri+"/package_with_good_signature!//index.html");
  channel.notificationCallbacks = new LoadContextCallback(1024, false, false, false);
  channel.asyncOpen(new Listener(function(l) {
    do_check_true(l.gotStopRequestOK);
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
  Services.prefs.setBoolPref("network.http.packaged-signed-apps-enabled", originalSignedAppEnabled);
}
