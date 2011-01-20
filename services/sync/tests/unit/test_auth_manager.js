Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/util.js");

let logger;

function server_handler(metadata, response) {
  let body, statusCode, status;

  switch (metadata.getHeader("Authorization")) {
    // guest:guest
    case "Basic Z3Vlc3Q6Z3Vlc3Q=":
      body = "This path exists and is protected";
      statusCode = 200;
      status = "OK";
      break;
    // johndoe:moneyislike$\u20ac\xa5\u5143
    case "Basic am9obmRvZTptb25leWlzbGlrZSTigqzCpeWFgw==":
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
do_test_pending();
  logger = Log4Moz.repository.getLogger('Test');
  Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

  let server = new nsHttpServer();
  server.registerPathHandler("/foo", server_handler);
  server.registerPathHandler("/bar", server_handler);
  server.start(8080);

  let auth = new BasicAuthenticator(new Identity("secret", "guest", "guest"));
  let auth2 = new BasicAuthenticator(
      new Identity("secret2", "johndoe", "moneyislike$\u20ac\xa5\u5143"));
  Auth.defaultAuthenticator = auth;
  Auth.registerAuthenticator("bar$", auth2);

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
