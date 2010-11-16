Cu.import("resource://services-sync/auth.js");
Cu.import("resource://services-sync/ext/Observers.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/util.js");

let logger;

function server_open(metadata, response) {
  let body;
  if (metadata.method == "GET") {
    body = "This path exists";
    response.setStatusLine(metadata.httpVersion, 200, "OK");
  } else {
    body = "Wrong request method";
    response.setStatusLine(metadata.httpVersion, 405, "Method Not Allowed");
  }
  response.bodyOutputStream.write(body, body.length);
}

function server_protected(metadata, response) {
  let body;

  // no btoa() in xpcshell.  it's guest:guest
  if (metadata.hasHeader("Authorization") &&
      metadata.getHeader("Authorization") == "Basic Z3Vlc3Q6Z3Vlc3Q=") {
    body = "This path exists and is protected";
    response.setStatusLine(metadata.httpVersion, 200, "OK, authorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);
  } else {
    body = "This path exists and is protected - failed";
    response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);
  }

  response.bodyOutputStream.write(body, body.length);
}

function server_404(metadata, response) {
  let body = "File not found";
  response.setStatusLine(metadata.httpVersion, 404, "Not Found");
  response.bodyOutputStream.write(body, body.length);
}


let sample_data = {
  some: "sample_data",
  injson: "format",
  number: 42
};

function server_upload(metadata, response) {
  let body;

  let input = readBytesFromInputStream(metadata.bodyInputStream);
  if (input == JSON.stringify(sample_data)) {
    body = "Valid data upload via " + metadata.method;
    response.setStatusLine(metadata.httpVersion, 200, "OK");
  } else {
    body = "Invalid data upload via " + metadata.method + ': ' + input;
    response.setStatusLine(metadata.httpVersion, 500, "Internal Server Error");
  }

  response.bodyOutputStream.write(body, body.length);
}

function server_delete(metadata, response) {
  let body;
  if (metadata.method == "DELETE") {
    body = "This resource has been deleted";
    response.setStatusLine(metadata.httpVersion, 200, "OK");
  } else {
    body = "Wrong request method";
    response.setStatusLine(metadata.httpVersion, 405, "Method Not Allowed");
  }
  response.bodyOutputStream.write(body, body.length);
}

function server_json(metadata, response) {
  let body = JSON.stringify(sample_data);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.bodyOutputStream.write(body, body.length);
}

const TIMESTAMP = 1274380461;

function server_timestamp(metadata, response) {
  let body = "Thank you for your request";
  response.setHeader("X-Weave-Timestamp", ''+TIMESTAMP, false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.bodyOutputStream.write(body, body.length);
}

function server_backoff(metadata, response) {
  let body = "Hey, back off!";
  response.setHeader("X-Weave-Backoff", '600', false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.bodyOutputStream.write(body, body.length);  
}

function server_quota_notice(request, response) {
  let body = "You're approaching quota.";
  response.setHeader("X-Weave-Quota-Remaining", '1048576', false);
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.bodyOutputStream.write(body, body.length);  
}

function server_quota_error(request, response) {
  let body = "14";
  response.setHeader("X-Weave-Quota-Remaining", '-1024', false);
  response.setStatusLine(request.httpVersion, 400, "OK");
  response.bodyOutputStream.write(body, body.length);  
}

function server_headers(metadata, response) {
  let ignore_headers = ["host", "user-agent", "accept", "accept-language",
                        "accept-encoding", "accept-charset", "keep-alive",
                        "connection", "pragma", "cache-control",
                        "content-length"];
  let headers = metadata.headers;
  let header_names = [];
  while (headers.hasMoreElements()) {
    let header = headers.getNext().toString();
    if (ignore_headers.indexOf(header) == -1) {
      header_names.push(header);
    }
  }
  header_names = header_names.sort();

  headers = {};
  for each (let header in header_names) {
    headers[header] = metadata.getHeader(header);
  }
  let body = JSON.stringify(headers);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.bodyOutputStream.write(body, body.length);
}


function run_test() {
  logger = Log4Moz.repository.getLogger('Test');
  Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

  do_test_pending();
  let server = httpd_setup({
    "/open": server_open,
    "/protected": server_protected,
    "/404": server_404,
    "/upload": server_upload,
    "/delete": server_delete,
    "/json": server_json,
    "/timestamp": server_timestamp,
    "/headers": server_headers,
    "/backoff": server_backoff,
    "/quota-notice": server_quota_notice,
    "/quota-error": server_quota_error
  });

  Utils.prefs.setIntPref("network.numRetries", 1); // speed up test

  let did401 = false;
  Observers.add("weave:resource:status:401", function() did401 = true);

  let quotaValue;
  Observers.add("weave:service:quota:remaining",
                function (subject) quotaValue = subject);


  // Ensure exceptions from inside callbacks leads to test failures.
  function ensureThrows(func) {
    return function() {
      try {
        func.apply(this, arguments);
      } catch (ex) {
        do_throw(ex);
      }
    };
  }

  let res_upload = new AsyncResource("http://localhost:8080/upload");
  let res_headers = new AsyncResource("http://localhost:8080/headers");

  _("Resource object memebers");
  let res = new AsyncResource("http://localhost:8080/open");
  do_check_true(res.uri instanceof Ci.nsIURI);
  do_check_eq(res.uri.spec, "http://localhost:8080/open");
  do_check_eq(res.spec, "http://localhost:8080/open");
  do_check_eq(typeof res.headers, "object");
  do_check_eq(typeof res.authenticator, "object");
  // Initially res.data is null since we haven't performed a GET or
  // PUT/POST request yet.
  do_check_eq(res.data, null);

  Utils.asyncChain(function (next) {

    _("GET a non-password-protected resource");
    do_test_pending();
    res.get(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, "This path exists");
      do_check_eq(content.status, 200);
      do_check_true(content.success);
      // res.data has been updated with the result from the request
      do_check_eq(res.data, content);

      // Since we didn't receive proper JSON data, accessing content.obj
      // will result in a SyntaxError from JSON.parse
      let didThrow = false;
      try {
        content.obj;
      } catch (ex) {
        didThrow = true;
      }
      do_check_true(didThrow);

      do_test_finished();
      next();
    }));

  }, function (next) {

    _("GET a password protected resource (test that it'll fail w/o pass, no throw)");
    let res2 = new AsyncResource("http://localhost:8080/protected");
    do_test_pending();
    res2.get(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_true(did401);
      do_check_eq(content, "This path exists and is protected - failed");
      do_check_eq(content.status, 401);
      do_check_false(content.success);
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("GET a password protected resource");
    let auth = new BasicAuthenticator(new Identity("secret", "guest", "guest"));
    let res3 = new AsyncResource("http://localhost:8080/protected");
    res3.authenticator = auth;
    do_check_eq(res3.authenticator, auth);
    do_test_pending();
    res3.get(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, "This path exists and is protected");
      do_check_eq(content.status, 200);
      do_check_true(content.success);
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("GET a non-existent resource (test that it'll fail, but not throw)");
    let res4 = new AsyncResource("http://localhost:8080/404");
    do_test_pending();
    res4.get(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, "File not found");
      do_check_eq(content.status, 404);
      do_check_false(content.success);

      // Check some headers of the 404 response
      do_check_eq(content.headers.connection, "close");
      do_check_eq(content.headers.server, "httpd.js");
      do_check_eq(content.headers["content-length"], 14);

      do_test_finished();
      next();
    }));

  }, function (next) {

    _("PUT to a resource (string)");
    do_test_pending();
    res_upload.put(JSON.stringify(sample_data), ensureThrows(function(error, content) {
      do_check_eq(error, null);
      do_check_eq(content, "Valid data upload via PUT");
      do_check_eq(content.status, 200);
      do_check_eq(res_upload.data, content);
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("PUT to a resource (object)");
    do_test_pending();
    res_upload.put(sample_data, ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, "Valid data upload via PUT");
      do_check_eq(content.status, 200);
      do_check_eq(res_upload.data, content);
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("PUT without data arg (uses resource.data) (string)");
    do_test_pending();
    res_upload.data = JSON.stringify(sample_data);
    res_upload.put(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, "Valid data upload via PUT");
      do_check_eq(content.status, 200);
      do_check_eq(res_upload.data, content);
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("PUT without data arg (uses resource.data) (object)");
    do_test_pending();
    res_upload.data = sample_data;
    res_upload.put(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, "Valid data upload via PUT");
      do_check_eq(content.status, 200);
      do_check_eq(res_upload.data, content);
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("POST to a resource (string)");
    do_test_pending();
    res_upload.post(JSON.stringify(sample_data), ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, "Valid data upload via POST");
      do_check_eq(content.status, 200);
      do_check_eq(res_upload.data, content);
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("POST to a resource (object)");
    do_test_pending();
    res_upload.post(sample_data, ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, "Valid data upload via POST");
      do_check_eq(content.status, 200);
      do_check_eq(res_upload.data, content);
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("POST without data arg (uses resource.data) (string)");
    do_test_pending();
    res_upload.data = JSON.stringify(sample_data);
    res_upload.post(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, "Valid data upload via POST");
      do_check_eq(content.status, 200);
      do_check_eq(res_upload.data, content);
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("POST without data arg (uses resource.data) (object)");
    do_test_pending();
    res_upload.data = sample_data;
    res_upload.post(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, "Valid data upload via POST");
      do_check_eq(content.status, 200);
      do_check_eq(res_upload.data, content);
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("DELETE a resource");
    do_test_pending();
    let res6 = new AsyncResource("http://localhost:8080/delete");
    res6.delete(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, "This resource has been deleted");
      do_check_eq(content.status, 200);
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("JSON conversion of response body");
    do_test_pending();
    let res7 = new AsyncResource("http://localhost:8080/json");
    res7.get(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, JSON.stringify(sample_data));
      do_check_eq(content.status, 200);
      do_check_eq(JSON.stringify(content.obj), JSON.stringify(sample_data));
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("X-Weave-Timestamp header updates Resource.serverTime");
    do_test_pending();
    // Before having received any response containing the
    // X-Weave-Timestamp header, Resource.serverTime is null.
    do_check_eq(Resource.serverTime, null);
    let res8 = new AsyncResource("http://localhost:8080/timestamp");
    res8.get(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(Resource.serverTime, TIMESTAMP);
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("GET: no special request headers");
    do_test_pending();
    res_headers.get(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, '{}');
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("PUT: Content-Type defaults to text/plain");
    do_test_pending();
    res_headers.put('data', ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, JSON.stringify({"content-type": "text/plain"}));
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("POST: Content-Type defaults to text/plain");
    do_test_pending();
    res_headers.post('data', ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, JSON.stringify({"content-type": "text/plain"}));
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("setHeader(): setting simple header");
    do_test_pending();
    res_headers.setHeader('X-What-Is-Weave', 'awesome');
    do_check_eq(res_headers.headers['x-what-is-weave'], 'awesome');
    res_headers.get(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, JSON.stringify({"x-what-is-weave": "awesome"}));
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("setHeader(): setting multiple headers, overwriting existing header");
    do_test_pending();
    res_headers.setHeader('X-WHAT-is-Weave', 'more awesomer',
                   'X-Another-Header', 'hello world');
    do_check_eq(res_headers.headers['x-what-is-weave'], 'more awesomer');
    do_check_eq(res_headers.headers['x-another-header'], 'hello world');
    res_headers.get(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, JSON.stringify({"x-another-header": "hello world",
                                           "x-what-is-weave": "more awesomer"}));
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("Setting headers object");
    do_test_pending();
    res_headers.headers = {};
    res_headers.get(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, "{}");
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("PUT: override default Content-Type");
    do_test_pending();
    res_headers.setHeader('Content-Type', 'application/foobar');
    do_check_eq(res_headers.headers['content-type'], 'application/foobar');
    res_headers.put('data', ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, JSON.stringify({"content-type": "application/foobar"}));
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("POST: override default Content-Type");
    do_test_pending();
    res_headers.post('data', ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content, JSON.stringify({"content-type": "application/foobar"}));
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("X-Weave-Backoff header notifies observer");
    let backoffInterval;
    function onBackoff(subject, data) {
      backoffInterval = subject;
    }
    Observers.add("weave:service:backoff:interval", onBackoff);

    do_test_pending();
    let res10 = new AsyncResource("http://localhost:8080/backoff");
    res10.get(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(backoffInterval, 600);
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("X-Weave-Quota-Remaining header notifies observer on successful requests.");
    do_test_pending();
    let res10 = new AsyncResource("http://localhost:8080/quota-error");
    res10.get(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content.status, 400);
      do_check_eq(quotaValue, undefined); // HTTP 400, so no observer notification.
      do_test_finished();
      next();
    }));

  }, function (next) {

    do_test_pending();
    let res10 = new AsyncResource("http://localhost:8080/quota-notice");
    res10.get(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(content.status, 200);
      do_check_eq(quotaValue, 1048576);
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("Error handling in ChannelListener etc. preserves exception information");
    do_test_pending();
    let res11 = new AsyncResource("http://localhost:12345/does/not/exist");
    res11.get(ensureThrows(function (error, content) {
      do_check_neq(error, null);
      do_check_eq(error.message, "NS_ERROR_CONNECTION_REFUSED");
      do_check_eq(typeof error.stack, "string");
      do_test_finished();
      next();
    }));

  }, function (next) {

    let redirRequest;
    let redirToOpen = function(subject) {
      subject.newUri = "http://localhost:8080/open";
      redirRequest = subject;
    };
    Observers.add("weave:resource:status:401", redirToOpen);

    _("Notification of 401 can redirect to another uri");
    did401 = false;
    do_test_pending();
    let res12 = new AsyncResource("http://localhost:8080/protected");
    res12.get(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_eq(res12.spec, "http://localhost:8080/open");
      do_check_eq(content, "This path exists");
      do_check_eq(content.status, 200);
      do_check_true(content.success);
      do_check_eq(res.data, content);
      do_check_true(did401);
      do_check_eq(redirRequest.response, "This path exists and is protected - failed");
      do_check_eq(redirRequest.response.status, 401);
      do_check_false(redirRequest.response.success);

      Observers.remove("weave:resource:status:401", redirToOpen);
      do_test_finished();
      next();
    }));

  }, function (next) {

    _("Removing the observer should result in the original 401");
    did401 = false;
    let res13 = new AsyncResource("http://localhost:8080/protected");
    res13.get(ensureThrows(function (error, content) {
      do_check_eq(error, null);
      do_check_true(did401);
      do_check_eq(content, "This path exists and is protected - failed");
      do_check_eq(content.status, 401);
      do_check_false(content.success);
      do_test_finished();
      next();
    }));

  }, function (next) {

    // Don't quit test harness before server shuts down.
    server.stop(do_test_finished);

  })();

}
