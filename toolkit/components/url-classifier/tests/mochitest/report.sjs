/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const SJS = "report.sjs?";
const REDIRECT = "mochi.test:8888/chrome/toolkit/components/url-classifier/tests/mochitest/" + SJS;

Components.utils.importGlobalProperties(["URLSearchParams"]);

function createBlockedIframePage() {
  return `<!DOCTYPE HTML>
          <html>
          <head>
            <title></title>
          </head>
          <body>
            <iframe id="phishingFrame" ></iframe>
          </body>
          </html>`;
}

function createPage() {
  return `<!DOCTYPE HTML>
          <html>
          <head>
            <title>Hello World</title>
          </head>
          <body>
            <script></script>
          </body>
          </html>`;
}

function handleRequest(request, response)
{
  var params = new URLSearchParams(request.queryString);
  var action = params.get("action");

  if (action === "create-blocked-iframe") {
    response.setHeader('Cache-Control', 'no-cache', false);
    response.setHeader('Content-Type', 'text/html; charset=utf-8', false);
    response.write(createBlockedIframePage());
    return;
  }

  if (action === "redirect") {
    response.setHeader('Cache-Control', 'no-cache', false);
    response.setHeader('Content-Type', 'text/html; charset=utf-8', false);
    response.write(createPage());
    return;
  }

  if (action === "reporturl") {
    response.setHeader('Cache-Control', 'no-cache', false);
    response.setHeader('Content-Type', 'text/html; charset=utf-8', false);
    response.write(createPage());
    return;
  }

  if (action === "create-blocked-redirect") {
    params.delete("action");
    params.append("action", "redirect");
    response.setStatusLine("1.1", 302, "found");
    response.setHeader("Location",  "https://" + REDIRECT + params.toString(), false);
    return;
  }

  response.write("I don't know action " + action);
  return;
}
