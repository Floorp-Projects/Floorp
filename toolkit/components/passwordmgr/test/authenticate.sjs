"use strict";

function handleRequest(request, response) {
  try {
    reallyHandleRequest(request, response);
  } catch (e) {
    response.setStatusLine("1.0", 200, "AlmostOK");
    response.write("Error handling request: " + e);
  }
}

function reallyHandleRequest(request, response) {
  let match;
  let requestAuth = true,
    requestProxyAuth = true;

  // Allow the caller to drive how authentication is processed via the query.
  // Eg, http://localhost:8888/authenticate.sjs?user=foo&realm=bar
  // The extra ? allows the user/pass/realm checks to succeed if the name is
  // at the beginning of the query string.
  let query = "?" + request.queryString;

  let expected_user = "",
    expected_pass = "",
    realm = "mochitest";
  let proxy_expected_user = "",
    proxy_expected_pass = "",
    proxy_realm = "mochi-proxy";
  let huge = false,
    plugin = false,
    anonymous = false,
    formauth = false;
  let authHeaderCount = 1;
  // user=xxx
  match = /[^_]user=([^&]*)/.exec(query);
  if (match) {
    expected_user = match[1];
  }

  // pass=xxx
  match = /[^_]pass=([^&]*)/.exec(query);
  if (match) {
    expected_pass = match[1];
  }

  // realm=xxx
  match = /[^_]realm=([^&]*)/.exec(query);
  if (match) {
    realm = match[1];
  }

  // proxy_user=xxx
  match = /proxy_user=([^&]*)/.exec(query);
  if (match) {
    proxy_expected_user = match[1];
  }

  // proxy_pass=xxx
  match = /proxy_pass=([^&]*)/.exec(query);
  if (match) {
    proxy_expected_pass = match[1];
  }

  // proxy_realm=xxx
  match = /proxy_realm=([^&]*)/.exec(query);
  if (match) {
    proxy_realm = match[1];
  }

  // huge=1
  match = /huge=1/.exec(query);
  if (match) {
    huge = true;
  }

  // plugin=1
  match = /plugin=1/.exec(query);
  if (match) {
    plugin = true;
  }

  // multiple=1
  match = /multiple=([^&]*)/.exec(query);
  if (match) {
    authHeaderCount = match[1] + 0;
  }

  // anonymous=1
  match = /anonymous=1/.exec(query);
  if (match) {
    anonymous = true;
  }

  // formauth=1
  match = /formauth=1/.exec(query);
  if (match) {
    formauth = true;
  }

  // Look for an authentication header, if any, in the request.
  //
  // EG: Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==
  //
  // This test only supports Basic auth. The value sent by the client is
  // "username:password", obscured with base64 encoding.

  let actual_user = "",
    actual_pass = "",
    authHeader,
    authPresent = false;
  if (request.hasHeader("Authorization")) {
    authPresent = true;
    authHeader = request.getHeader("Authorization");
    match = /Basic (.+)/.exec(authHeader);
    if (match.length != 2) {
      throw new Error("Couldn't parse auth header: " + authHeader);
    }

    let userpass = atob(match[1]);
    match = /(.*):(.*)/.exec(userpass);
    if (match.length != 3) {
      throw new Error("Couldn't decode auth header: " + userpass);
    }
    actual_user = match[1];
    actual_pass = match[2];
  }

  let proxy_actual_user = "",
    proxy_actual_pass = "";
  if (request.hasHeader("Proxy-Authorization")) {
    authHeader = request.getHeader("Proxy-Authorization");
    match = /Basic (.+)/.exec(authHeader);
    if (match.length != 2) {
      throw new Error("Couldn't parse auth header: " + authHeader);
    }

    let userpass = atob(match[1]);
    match = /(.*):(.*)/.exec(userpass);
    if (match.length != 3) {
      throw new Error("Couldn't decode auth header: " + userpass);
    }
    proxy_actual_user = match[1];
    proxy_actual_pass = match[2];
  }

  // Don't request authentication if the credentials we got were what we
  // expected.
  if (expected_user == actual_user && expected_pass == actual_pass) {
    requestAuth = false;
  }
  if (
    proxy_expected_user == proxy_actual_user &&
    proxy_expected_pass == proxy_actual_pass
  ) {
    requestProxyAuth = false;
  }

  if (anonymous) {
    if (authPresent) {
      response.setStatusLine(
        "1.0",
        400,
        "Unexpected authorization header found"
      );
    } else {
      response.setStatusLine("1.0", 200, "Authorization header not found");
    }
  } else if (requestProxyAuth) {
    response.setStatusLine("1.0", 407, "Proxy authentication required");
    for (let i = 0; i < authHeaderCount; ++i) {
      response.setHeader(
        "Proxy-Authenticate",
        'basic realm="' + proxy_realm + '"',
        true
      );
    }
  } else if (requestAuth) {
    if (formauth && authPresent) {
      response.setStatusLine("1.0", 403, "Form authentication required");
    } else {
      response.setStatusLine("1.0", 401, "Authentication required");
    }
    for (let i = 0; i < authHeaderCount; ++i) {
      response.setHeader(
        "WWW-Authenticate",
        'basic realm="' + realm + '"',
        true
      );
    }
  } else {
    response.setStatusLine("1.0", 200, "OK");
  }

  response.setHeader("Content-Type", "application/xhtml+xml", false);
  response.write("<html xmlns='http://www.w3.org/1999/xhtml'>");
  response.write(
    "<p>Login: <span id='ok'>" +
      (requestAuth ? "FAIL" : "PASS") +
      "</span></p>\n"
  );
  response.write(
    "<p>Proxy: <span id='proxy'>" +
      (requestProxyAuth ? "FAIL" : "PASS") +
      "</span></p>\n"
  );
  response.write("<p>Auth: <span id='auth'>" + authHeader + "</span></p>\n");
  response.write("<p>User: <span id='user'>" + actual_user + "</span></p>\n");
  response.write("<p>Pass: <span id='pass'>" + actual_pass + "</span></p>\n");

  if (huge) {
    response.write("<div style='display: none'>");
    for (let i = 0; i < 100000; i++) {
      response.write("123456789\n");
    }
    response.write("</div>");
    response.write(
      "<span id='footnote'>This is a footnote after the huge content fill</span>"
    );
  }

  if (plugin) {
    response.write(
      "<embed id='embedtest' style='width: 400px; height: 100px;' " +
        "type='application/x-test'></embed>\n"
    );
  }

  response.write("</html>");
}
