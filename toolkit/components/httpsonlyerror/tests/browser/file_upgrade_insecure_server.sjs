// Serverside Javascript for browser_exception.js
// Bug 1625156 - Error page for HTTPS Only Mode

const expectedQueries = ["content", "img", "iframe", "xhr", "nestedimg"];
const TOTAL_EXPECTED_REQUESTS = expectedQueries.length;

const CONTENT = path => `
<!DOCTYPE HTML>
<html>
  <head>
    <meta charset='utf-8'>
  </head>
  <body>
    <p>Insecure website</p>
    <script type="application/javascript">
      var myXHR = new XMLHttpRequest();
      myXHR.open("GET", "${path}file_upgrade_insecure_server.sjs?xhr");
      myXHR.send(null);
    </script>
    <img src='${path}file_upgrade_insecure_server.sjs?img'></img>
    <iframe src="${path}file_upgrade_insecure_server.sjs?iframe"></iframe>
  </body>
</html>`;

const IFRAME_CONTENT = path => `
<!DOCTYPE HTML>
<html>
  <head>
    <meta charset='utf-8'>
  </head>
  <body>
    <p>Nested insecure website</p>
    <img src='${path}file_upgrade_insecure_server.sjs?nestedimg'></img>
  </body>
</html>`;

function handleRequest(request, response) {
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  var queryString = request.queryString;

  // initialize server variables and save the object state
  // of the initial request, which returns async once the
  // server has processed all requests.
  if (queryString.startsWith("queryresult")) {
    response.processAsync();
    setState("totaltests", TOTAL_EXPECTED_REQUESTS.toString());
    setState("receivedQueries", "");
    setState("rootPath", /=(.+)/.exec(queryString)[1]);
    setObjectState("queryResult", response);
    return;
  }

  // just in case error handling for unexpected queries
  if (!expectedQueries.includes(queryString)) {
    response.write("unexpected-response");
    return;
  }

  // make sure all the requested queries are indeed http
  const testResult =
    queryString + (request.scheme == "http" ? "-ok" : "-error");

  var receivedQueries = getState("receivedQueries");

  // images, scripts, etc. get queried twice, do not
  // confuse the server by storing the preload as
  // well as the actual load. If either the preload
  // or the actual load is not https, then we would
  // append "-error" in the array and the test would
  // fail at the end.
  if (receivedQueries.includes(testResult)) {
    return;
  }

  // append the result to the total query string array
  if (receivedQueries != "") {
    receivedQueries += ",";
  }
  receivedQueries += testResult;
  setState("receivedQueries", receivedQueries);

  // keep track of how many more requests the server
  // is expecting
  var totaltests = parseInt(getState("totaltests"));
  totaltests -= 1;
  setState("totaltests", totaltests.toString());

  // Respond with html content
  if (queryString == "content") {
    response.write(CONTENT(getState("rootPath")));
  } else if (queryString == "iframe") {
    response.write(IFRAME_CONTENT(getState("rootPath")));
  }

  // if we have received all the requests, we return
  // the result back.
  if (totaltests == 0) {
    getObjectState("queryResult", function(queryResponse) {
      if (!queryResponse) {
        return;
      }
      var receivedQueries = getState("receivedQueries");
      queryResponse.write(receivedQueries);
      queryResponse.finish();
    });
  }
}
