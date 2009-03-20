let Httpd = {};
Cu.import("resource://harness/modules/httpd.js", Httpd);

function httpd_setup (handlers) {
  let server = new Httpd.nsHttpServer();
  for (let path in handlers) {
    server.registerPathHandler(path, handlers[path]);
  }
  server.start(8080);
  return server;
}

function httpd_basic_auth_handler(body, metadata, response) {
  // no btoa() in xpcshell.  it's guest:guest
  if (metadata.hasHeader("Authorization") &&
      metadata.getHeader("Authorization") == "Basic Z3Vlc3Q6Z3Vlc3Q=") {
    response.setStatusLine(metadata.httpVersion, 200, "OK, authorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);
  } else {
    body = "This path exists and is protected - failed";
    response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);
  }
  response.bodyOutputStream.write(body, body.length);
}
