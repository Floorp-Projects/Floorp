//
// If a response to a non-safe HTTP request-method contains the Location- or
// Content-Location header, we must make sure to invalidate any cached entry
// representing the URIs pointed to by either header. RFC 2616 section 13.10
//
// This test uses 3 URIs: "/post" is the target of a POST-request and always
// redirects (301) to "/redirect". The URIs "/redirect" and "/cl" both counts
// the number of loads from the server (handler). The response from "/post"
// always contains the headers "Location: /redirect" and "Content-Location:
// /cl", whose cached entries are to be invalidated. The tests verifies that
// "/redirect" and "/cl" are loaded from server the expected number of times.
//

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

var httpserv;

function setupChannel(path) {
  return NetUtil.newChannel({uri: path, loadUsingSystemPrincipal: true})
                .QueryInterface(Ci.nsIHttpChannel);
}

// Verify that Content-Location-URI has been loaded once, load post_target
function InitialListener() { }
InitialListener.prototype = {
    onStartRequest: function(request) { },
    onStopRequest: function(request, status) {
        Assert.equal(1, numberOfCLHandlerCalls);
        executeSoon(function() {
            var channel = setupChannel("http://localhost:" +
                                       httpserv.identity.primaryPort + "/post");
            channel.requestMethod = "POST";
            channel.asyncOpen(new RedirectingListener());
        });
    }
};

// Verify that Location-URI has been loaded once, reload post_target
function RedirectingListener() { }
RedirectingListener.prototype = {
    onStartRequest: function(request) { },
    onStopRequest: function(request, status) {
        Assert.equal(1, numberOfHandlerCalls);
        executeSoon(function() {
            var channel = setupChannel("http://localhost:" +
                                       httpserv.identity.primaryPort + "/post");
            channel.requestMethod = "POST";
            channel.asyncOpen(new VerifyingListener());
        });
    }
};

// Verify that Location-URI has been loaded twice (cached entry invalidated),
// reload Content-Location-URI
function VerifyingListener() { }
VerifyingListener.prototype = {
    onStartRequest: function(request) { },
    onStopRequest: function(request, status) {
        Assert.equal(2, numberOfHandlerCalls);
        var channel = setupChannel("http://localhost:" +
                                   httpserv.identity.primaryPort + "/cl");
        channel.asyncOpen(new FinalListener());
    }
};

// Verify that Location-URI has been loaded twice (cached entry invalidated),
// stop test
function FinalListener() { }
FinalListener.prototype = {
    onStartRequest: function(request) { },
    onStopRequest: function(request, status) {
        Assert.equal(2, numberOfCLHandlerCalls);
        httpserv.stop(do_test_finished);
    }
};

function run_test() {
  httpserv = new HttpServer();
  httpserv.registerPathHandler("/cl", content_location);
  httpserv.registerPathHandler("/post", post_target);
  httpserv.registerPathHandler("/redirect", redirect_target);
  httpserv.start(-1);

  // Clear cache
  evict_cache_entries();

  // Load Content-Location URI into cache and start the chain of loads
  var channel = setupChannel("http://localhost:" +
                             httpserv.identity.primaryPort + "/cl");
  channel.asyncOpen(new InitialListener());

  do_test_pending();
}

var numberOfCLHandlerCalls = 0;
function content_location(metadata, response) {
    numberOfCLHandlerCalls++;
    response.setStatusLine(metadata.httpVersion, 200, "Ok");
    response.setHeader("Cache-Control", "max-age=360000", false);
}

function post_target(metadata, response) {
    response.setStatusLine(metadata.httpVersion, 301, "Moved Permanently");
    response.setHeader("Location", "/redirect", false);
    response.setHeader("Content-Location", "/cl", false);
    response.setHeader("Cache-Control", "max-age=360000", false);
}

var numberOfHandlerCalls = 0;
function redirect_target(metadata, response) {
    numberOfHandlerCalls++;
    response.setStatusLine(metadata.httpVersion, 200, "Ok");
    response.setHeader("Cache-Control", "max-age=360000", false);
}
