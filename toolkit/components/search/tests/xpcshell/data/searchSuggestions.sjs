/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

Cu.importGlobalProperties(["TextEncoder"]);

/**
 * Provide search suggestions in the OpenSearch JSON format.
 */

function handleRequest(request, response) {
  // Get the query parameters from the query string.
  let query = parseQueryString(request.queryString);

  function convertToUtf8(str) {
    return String.fromCharCode(...new TextEncoder().encode(str));
  }

  function writeSuggestions(query, completions = []) {
    let jsonString = JSON.stringify([query, completions]);

    // This script must be evaluated as UTF-8 for this to write out the bytes of
    // the string in UTF-8.  If it's evaluated as Latin-1, the written bytes
    // will be the result of UTF-8-encoding the result-string *twice*, which
    // will break the "I ❤️" case further down.
    let stringOfUtf8Bytes = convertToUtf8(jsonString);

    response.write(stringOfUtf8Bytes);
  }

  /**
   * Sends `data` as suggestions directly. This is useful when testing rich
   * suggestions, which do not conform to the object shape sent by
   * writeSuggestions.
   *
   * @param {array} data
   */
  function writeSuggestionsDirectly(data) {
    let jsonString = JSON.stringify(data);
    let stringOfUtf8Bytes = convertToUtf8(jsonString);
    response.setHeader("Content-Type", "application/json", false);
    response.write(stringOfUtf8Bytes);
  }

  response.setStatusLine(request.httpVersion, 200, "OK");

  let q = request.method == "GET" ? query.q : undefined;
  if (q == "cookie") {
    response.setHeader("Set-Cookie", "cookie=1");
    writeSuggestions(q);
  } else if (q == "no remote" || q == "no results") {
    writeSuggestions(q);
  } else if (q == "Query Mismatch") {
    writeSuggestions("This is an incorrect query string", ["some result"]);
  } else if (q == "Query Case Mismatch") {
    writeSuggestions(q.toUpperCase(), [q]);
  } else if (q == "") {
    writeSuggestions("", ["The server should never be sent an empty query"]);
  } else if (q && q.startsWith("mo")) {
    writeSuggestions(q, ["Mozilla", "modern", "mom"]);
  } else if (q && q.startsWith("I ❤️")) {
    writeSuggestions(q, ["I ❤️ Mozilla"]);
  } else if (q && q.startsWith("tailjunk ")) {
    writeSuggestionsDirectly([
      q,
      [q + " normal", q + " tail 1", q + " tail 2"],
      [],
      {
        "google:irrelevantparameter": [],
        "google:badformat": {
          "google:suggestdetail": [
            {},
            { mp: "… ", t: "tail 1" },
            { mp: "… ", t: "tail 2" },
          ],
        },
      },
    ]);
  } else if (q && q.startsWith("tailjunk few ")) {
    writeSuggestionsDirectly([
      q,
      [q + " normal", q + " tail 1", q + " tail 2"],
      [],
      {
        "google:irrelevantparameter": [],
        "google:badformat": {
          "google:suggestdetail": [{ mp: "… ", t: "tail 1" }],
        },
      },
    ]);
  } else if (q && q.startsWith("tailalt ")) {
    writeSuggestionsDirectly([
      q,
      [q + " normal", q + " tail 1", q + " tail 2"],
      {
        "google:suggestdetail": [
          {},
          { mp: "… ", t: "tail 1" },
          { mp: "… ", t: "tail 2" },
        ],
      },
    ]);
  } else if (q && q.startsWith("tail ")) {
    writeSuggestionsDirectly([
      q,
      [q + " normal", q + " tail 1", q + " tail 2"],
      [],
      {
        "google:irrelevantparameter": [],
        "google:suggestdetail": [
          {},
          { mp: "… ", t: "tail 1" },
          { mp: "… ", t: "tail 2" },
        ],
      },
    ]);
  } else if (q && q.startsWith("richempty ")) {
    writeSuggestionsDirectly([
      q,
      [q + " normal", q + " tail 1", q + " tail 2"],
      [],
      {
        "google:irrelevantparameter": [],
        "google:suggestdetail": [],
      },
    ]);
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
    query[name] = decodeURIComponent(value).replace(/[+]/g, " ");
  });
  return query;
}
