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
  do_test_pending();

  logger = Log4Moz.repository.getLogger('Test');
  Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

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

  _("Resource object memebers");
  let res = new Resource("http://localhost:8080/open");
  do_check_true(res.uri instanceof Ci.nsIURI);
  do_check_eq(res.uri.spec, "http://localhost:8080/open");
  do_check_eq(res.spec, "http://localhost:8080/open");
  do_check_eq(typeof res.headers, "object");
  do_check_eq(typeof res.authenticator, "object");
  // Initially res.data is null since we haven't performed a GET or
  // PUT/POST request yet.
  do_check_eq(res.data, null);

  _("GET a non-password-protected resource");
  let content = res.get();
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

  let did401 = false;
  Observers.add("weave:resource:status:401", function() did401 = true);

  _("GET a password protected resource (test that it'll fail w/o pass, no throw)");
  let res2 = new Resource("http://localhost:8080/protected");
  content = res2.get();
  do_check_true(did401);
  do_check_eq(content, "This path exists and is protected - failed");
  do_check_eq(content.status, 401);
  do_check_false(content.success);

  _("GET a password protected resource");
  let auth = new BasicAuthenticator(new Identity("secret", "guest", "guest"));
  let res3 = new Resource("http://localhost:8080/protected");
  res3.authenticator = auth;
  do_check_eq(res3.authenticator, auth);
  content = res3.get();
  do_check_eq(content, "This path exists and is protected");
  do_check_eq(content.status, 200);
  do_check_true(content.success);

  _("GET a non-existent resource (test that it'll fail, but not throw)");
  let res4 = new Resource("http://localhost:8080/404");
  content = res4.get();
  do_check_eq(content, "File not found");
  do_check_eq(content.status, 404);
  do_check_false(content.success);

  // Check some headers of the 404 response
  do_check_eq(content.headers.connection, "close");
  do_check_eq(content.headers.server, "httpd.js");
  do_check_eq(content.headers["content-length"], 14);

  _("PUT to a resource (string)");
  let res5 = new Resource("http://localhost:8080/upload");
  content = res5.put(JSON.stringify(sample_data));
  do_check_eq(content, "Valid data upload via PUT");
  do_check_eq(content.status, 200);
  do_check_eq(res5.data, content);

  _("PUT to a resource (object)");
  content = res5.put(sample_data);
  do_check_eq(content, "Valid data upload via PUT");
  do_check_eq(content.status, 200);
  do_check_eq(res5.data, content);

  _("PUT without data arg (uses resource.data) (string)");
  res5.data = JSON.stringify(sample_data);
  content = res5.put();
  do_check_eq(content, "Valid data upload via PUT");
  do_check_eq(content.status, 200);
  do_check_eq(res5.data, content);

  _("PUT without data arg (uses resource.data) (object)");
  res5.data = sample_data;
  content = res5.put();
  do_check_eq(content, "Valid data upload via PUT");
  do_check_eq(content.status, 200);
  do_check_eq(res5.data, content);

  _("POST to a resource (string)");
  content = res5.post(JSON.stringify(sample_data));
  do_check_eq(content, "Valid data upload via POST");
  do_check_eq(content.status, 200);
  do_check_eq(res5.data, content);

  _("POST to a resource (object)");
  content = res5.post(sample_data);
  do_check_eq(content, "Valid data upload via POST");
  do_check_eq(content.status, 200);
  do_check_eq(res5.data, content);

  _("POST without data arg (uses resource.data) (string)");
  res5.data = JSON.stringify(sample_data);
  content = res5.post();
  do_check_eq(content, "Valid data upload via POST");
  do_check_eq(content.status, 200);
  do_check_eq(res5.data, content);

  _("POST without data arg (uses resource.data) (object)");
  res5.data = sample_data;
  content = res5.post();
  do_check_eq(content, "Valid data upload via POST");
  do_check_eq(content.status, 200);
  do_check_eq(res5.data, content);

  _("DELETE a resource");
  let res6 = new Resource("http://localhost:8080/delete");
  content = res6.delete();
  do_check_eq(content, "This resource has been deleted")
  do_check_eq(content.status, 200);

  _("JSON conversion of response body");
  let res7 = new Resource("http://localhost:8080/json");
  content = res7.get();
  do_check_eq(content, JSON.stringify(sample_data));
  do_check_eq(content.status, 200);
  do_check_eq(JSON.stringify(content.obj), JSON.stringify(sample_data));

  _("X-Weave-Timestamp header updates Resource.serverTime");
  // Before having received any response containing the
  // X-Weave-Timestamp header, Resource.serverTime is null.
  do_check_eq(Resource.serverTime, null);
  let res8 = new Resource("http://localhost:8080/timestamp");
  content = res8.get();
  do_check_eq(Resource.serverTime, TIMESTAMP);


  _("GET: no special request headers");
  let res9 = new Resource("http://localhost:8080/headers");
  content = res9.get();
  do_check_eq(content, '{}');

  _("PUT: Content-Type defaults to text/plain");
  content = res9.put('data');
  do_check_eq(content, JSON.stringify({"content-type": "text/plain"}));

  _("POST: Content-Type defaults to text/plain");
  content = res9.post('data');
  do_check_eq(content, JSON.stringify({"content-type": "text/plain"}));

  _("setHeader(): setting simple header");
  res9.setHeader('X-What-Is-Weave', 'awesome');
  do_check_eq(res9.headers['x-what-is-weave'], 'awesome');
  content = res9.get();
  do_check_eq(content, JSON.stringify({"x-what-is-weave": "awesome"}));

  _("setHeader(): setting multiple headers, overwriting existing header");
  res9.setHeader('X-WHAT-is-Weave', 'more awesomer',
                 'X-Another-Header', 'hello world');
  do_check_eq(res9.headers['x-what-is-weave'], 'more awesomer');
  do_check_eq(res9.headers['x-another-header'], 'hello world');
  content = res9.get();
  do_check_eq(content, JSON.stringify({"x-another-header": "hello world",
                                       "x-what-is-weave": "more awesomer"}));

  _("Setting headers object");
  res9.headers = {};
  content = res9.get();
  do_check_eq(content, "{}");

  _("PUT/POST: override default Content-Type");
  res9.setHeader('Content-Type', 'application/foobar');
  do_check_eq(res9.headers['content-type'], 'application/foobar');
  content = res9.put('data');
  do_check_eq(content, JSON.stringify({"content-type": "application/foobar"}));
  content = res9.post('data');
  do_check_eq(content, JSON.stringify({"content-type": "application/foobar"}));


  _("X-Weave-Backoff header notifies observer");
  let backoffInterval;
  function onBackoff(subject, data) {
    backoffInterval = subject;
  }
  Observers.add("weave:service:backoff:interval", onBackoff);

  let res10 = new Resource("http://localhost:8080/backoff");
  content = res10.get();
  do_check_eq(backoffInterval, 600);


  _("X-Weave-Quota-Remaining header notifies observer on successful requests.");
  let quotaValue;
  function onQuota(subject, data) {
    quotaValue = subject;
  }
  Observers.add("weave:service:quota:remaining", onQuota);

  res10 = new Resource("http://localhost:8080/quota-error");
  content = res10.get();
  do_check_eq(content.status, 400);
  do_check_eq(quotaValue, undefined); // HTTP 400, so no observer notification.

  res10 = new Resource("http://localhost:8080/quota-notice");
  content = res10.get();
  do_check_eq(content.status, 200);
  do_check_eq(quotaValue, 1048576);


  _("Error handling in _request() preserves exception information");
  let error;
  let res11 = new Resource("http://localhost:12345/does/not/exist");
  try {
    content = res11.get();
  } catch(ex) {
    error = ex;
  }
  do_check_eq(error.message, "NS_ERROR_CONNECTION_REFUSED");
  do_check_eq(typeof error.stack, "string");

  let redirRequest;
  let redirToOpen = function(subject) {
    subject.newUri = "http://localhost:8080/open";
    redirRequest = subject;
  };
  Observers.add("weave:resource:status:401", redirToOpen);

  _("Notification of 401 can redirect to another uri");
  did401 = false;
  let res12 = new Resource("http://localhost:8080/protected");
  content = res12.get();
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

  _("Removing the observer should result in the original 401");
  did401 = false;
  let res13 = new Resource("http://localhost:8080/protected");
  content = res13.get();
  do_check_true(did401);
  do_check_eq(content, "This path exists and is protected - failed");
  do_check_eq(content.status, 401);
  do_check_false(content.success);

  server.stop(do_test_finished);
}
