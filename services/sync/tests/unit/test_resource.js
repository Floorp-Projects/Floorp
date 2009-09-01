Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/resource.js");
Cu.import("resource://weave/auth.js");
Cu.import("resource://weave/identity.js");

let logger;
let Httpd = {};
Cu.import("resource://harness/modules/httpd.js", Httpd);

function server_open(metadata, response) {
  let body = "This path exists";
  response.setStatusLine(metadata.httpVersion, 200, "OK");
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

function run_test() {
  logger = Log4Moz.repository.getLogger('Test');
  Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

  let server = new Httpd.nsHttpServer();
  server.registerPathHandler("/open", server_open);
  server.registerPathHandler("/protected", server_protected);
  server.registerPathHandler("/404", server_404);
  server.start(8080);

  Utils.prefs.setIntPref("network.numRetries", 1); // speed up test

  // 1. A regular non-password-protected resource

  let res = new Resource("http://localhost:8080/open");
  let content = res.get();
  do_check_eq(content, "This path exists");
  do_check_eq(content.status, 200);
  do_check_true(content.success);

  // 2. A password protected resource (test that it'll fail w/o pass, no throw)
  let res2 = new Resource("http://localhost:8080/protected");
  content = res2.get();
  do_check_eq(content, "This path exists and is protected - failed")
  do_check_eq(content.status, 401);
  do_check_false(content.success);

  // 3. A password protected resource

  let auth = new BasicAuthenticator(new Identity("secret", "guest", "guest"));
  let res3 = new Resource("http://localhost:8080/protected");
  res3.authenticator = auth;
  content = res3.get();
  do_check_eq(content, "This path exists and is protected");
  do_check_eq(content.status, 200);
  do_check_true(content.success);

  // 4. A non-existent resource (test that it'll fail, but not throw)

  let res4 = new Resource("http://localhost:8080/404");
  content = res4.get();
  do_check_eq(content, "File not found");
  do_check_eq(content.status, 404);
  do_check_false(content.success);

  _("5. Check some headers of the 404 response");
  do_check_eq(content.headers.Connection, "close");
  do_check_eq(content.headers.Server, "httpd.js");
  do_check_eq(content.headers["Content-Length"], 14);

  // FIXME: additional coverage needed:
  // * PUT requests
  // * DELETE requests
  // * JsonFilter

  server.stop();
}
