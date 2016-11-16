/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
Components.utils.importGlobalProperties(["URLSearchParams", "URL"]);

function handleRequest(request, response) {
  let params = new URLSearchParams(request.queryString);
  if (params.has("no_redirect")) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write("ok");
  } else {
    response.setStatusLine(request.httpVersion, 302, "Moved Temporarily");
    let url = new URL(params.get("redirect_uri"));
    url.searchParams.set("access_token", "here ya go");
    response.setHeader("Location", url.href);
  }
}
