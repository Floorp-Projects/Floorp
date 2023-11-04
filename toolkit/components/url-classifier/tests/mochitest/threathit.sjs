/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const CC = Components.Constructor;
const BinaryInputStream = CC(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

function handleRequest(request, response) {
  let params = new URLSearchParams(request.queryString);
  var action = params.get("action");

  var responseBody;

  // Store data in the server side.
  if (action == "store") {
    // In the server side we will store:
    // All the full hashes or update for a given list
    let state = params.get("list") + params.get("type");
    let dataStr = params.get("data");
    setState(state, dataStr);
  } else if (action == "get") {
    let state = params.get("list") + params.get("type");
    responseBody = atob(getState(state));
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.bodyOutputStream.write(responseBody, responseBody.length);
  } else if (action == "report") {
    let state = params.get("list") + "report";
    let requestUrl =
      request.scheme +
      "://" +
      request.host +
      ":" +
      request.port +
      request.path +
      "?" +
      request.queryString;
    setState(state, requestUrl);
  } else if (action == "getreport") {
    let state = params.get("list") + "report";
    responseBody = getState(state);
    response.setHeader("Content-Type", "text/plain", false);
    response.write(responseBody);
  }
}
