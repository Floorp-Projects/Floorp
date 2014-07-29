/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

/**
 * Provide search suggestions in the OpenSearch JSON format.
 */

function handleRequest(request, response) {
  // Get the query parameters from the query string.
  let query = parseQueryString(request.queryString);

  function writeSuggestions(query, completions = []) {
    let result = [query, completions];
    response.write(JSON.stringify(result));
    return result;
  }

  response.setStatusLine(request.httpVersion, 200, "OK");

  let q = request.method == "GET" ? query.q : undefined;
  if (q == "no remote" || q == "no results") {
    writeSuggestions(q);
  } else if (q == "Query Mismatch") {
    writeSuggestions("This is an incorrect query string", ["some result"]);
  } else if (q == "") {
    writeSuggestions("", ["The server should never be sent an empty query"]);
  } else if (q && q.startsWith("mo")) {
    writeSuggestions(q, ["Mozilla", "modern", "mom"]);
  } else if (q && q.startsWith("I ❤️")) {
    writeSuggestions(q, ["I ❤️ Mozilla"]);
  } else if (q && q.startsWith("letter ")) {
    let letters = [];
    for (let charCode = "A".charCodeAt(); charCode <= "Z".charCodeAt(); charCode++) {
      letters.push("letter " + String.fromCharCode(charCode));
    }
    writeSuggestions(q, letters);
  } else if (q && q.startsWith("HTTP ")) {
    response.setStatusLine(request.httpVersion, q.replace("HTTP ", ""), q);
    writeSuggestions(q, [q]);
  } else if (q && q.startsWith("delay")) {
    // Delay the response by 200 milliseconds (less than the timeout but hopefully enough to abort
    // before completion).
    response.processAsync();
    writeSuggestions(q, [q]);
    setTimeout(() => response.finish(), 200);
  } else if (q && q.startsWith("slow ")) {
    // Delay the response by 10 seconds so the client timeout is reached.
    response.processAsync();
    writeSuggestions(q, [q]);
    setTimeout(() => response.finish(), 10000);
  } else if (request.method == "POST") {
    // This includes headers, not just the body
    let requestText = NetUtil.readInputStreamToString(request.bodyInputStream,
                                                      request.bodyInputStream.available());
    // Only use the last line which contains the encoded params
    let requestLines = requestText.split("\n");
    let postParams = parseQueryString(requestLines[requestLines.length - 1]);
    writeSuggestions(postParams.q, ["Mozilla", "modern", "mom"]);
  } else {
    response.setStatusLine(request.httpVersion, 404, "Not Found");
  }
}

function parseQueryString(queryString) {
  let query = {};
  queryString.split('&').forEach(function (val) {
    let [name, value] = val.split('=');
    query[name] = unescape(value).replace("+", " ");
  });
  return query;
}
