Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/util.js");

let logger;

function server_handler(metadata, response) {
  let body, statusCode, status;
  let guestHeader = basic_auth_header("guest", "guest");
  let johnHeader  = basic_auth_header("johndoe", "moneyislike$£¥");

  _("Guest header: " + guestHeader);
  _("John header:  " + johnHeader);
  
  switch (metadata.getHeader("Authorization")) {
    case guestHeader:
      body = "This path exists and is protected";
      statusCode = 200;
      status = "OK";
      break;
    case johnHeader:
      body = "This path exists and is protected by a UTF8 password";
      statusCode = 200;
      status = "OK";
      break;
    default:
      body = "This path exists and is protected - failed";
      statusCode = 401;
      status = "Unauthorized";
  }

  response.setStatusLine(metadata.httpVersion, statusCode, status);
  response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);
  response.bodyOutputStream.write(body, body.length);
}

function run_test() {
  initTestLogging("Trace");

  do_test_pending();
  let server = new nsHttpServer();
  server.registerPathHandler("/foo", server_handler);
  server.registerPathHandler("/bar", server_handler);
  server.start(8080);

  let guestIdentity = new Identity("secret", "guest", "guest");
  let johnIdentity  = new Identity("secret2", "johndoe", "moneyislike$£¥")
  let guestAuth     = new BasicAuthenticator(guestIdentity);
  let johnAuth      = new BasicAuthenticator(johnIdentity);
  Auth.defaultAuthenticator = guestAuth;
  Auth.registerAuthenticator("bar$", johnAuth);

  try {
    let content = new Resource("http://localhost:8080/foo").get();
    do_check_eq(content, "This path exists and is protected");
    do_check_eq(content.status, 200);

    content = new Resource("http://localhost:8080/bar").get();
    do_check_eq(content, "This path exists and is protected by a UTF8 password");
    do_check_eq(content.status, 200);
  } finally {
    server.stop(do_test_finished);
  }
}
