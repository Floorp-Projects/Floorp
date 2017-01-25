 /* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/utils.js");

function basic_auth_header(user, password) {
  return "Basic " + btoa(user + ":" + CommonUtils.encodeUTF8(password));
}

function basic_auth_matches(req, user, password) {
  if (!req.hasHeader("Authorization")) {
    return false;
  }

  let expected = basic_auth_header(user, CommonUtils.encodeUTF8(password));
  return req.getHeader("Authorization") == expected;
}

function httpd_basic_auth_handler(body, metadata, response) {
  if (basic_auth_matches(metadata, "guest", "guest")) {
    response.setStatusLine(metadata.httpVersion, 200, "OK, authorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);
  } else {
    body = "This path exists and is protected - failed";
    response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);
  }
  response.bodyOutputStream.write(body, body.length);
}
