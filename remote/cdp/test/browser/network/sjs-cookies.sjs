/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  const queryString = new URLSearchParams(request.queryString);

  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain; charset=utf-8", false);

  if (queryString.has("name") && queryString.has("value")) {
    const name = queryString.get("name");
    const value = queryString.get("value");
    const path = queryString.get("path") || "/";

    const expiry = queryString.get("expiry");
    const httpOnly = queryString.has("httpOnly");
    const secure = queryString.has("secure");
    const sameSite = queryString.get("sameSite");

    let cookie = `${name}=${value}; Path=${path}`;

    if (expiry) {
      cookie += `; Expires=${expiry}`;
    }

    if (httpOnly) {
      cookie += "; HttpOnly";
    }

    if (sameSite != undefined) {
      cookie += `; sameSite=${sameSite}`;
    }

    if (secure) {
      cookie += "; Secure";
    }

    response.setHeader("Set-Cookie", cookie, true);
    response.write(`Set cookie: ${cookie}`);
  }
}
