const CC = Components.Constructor;

Cu.import("resource://gre/modules/Timer.jsm");

const LocalFile = CC("@mozilla.org/file/local;1", "nsIFile",
                     "initWithPath");

const FileInputStream = CC("@mozilla.org/network/file-input-stream;1",
                           "nsIFileInputStream", "init");

const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream", "setInputStream");

function handleRequest(request, response) {
  let params = parseQueryString(request.queryString);

  response.setStatusLine(request.httpVersion, 200, "OK");

  // Compare and increment a cookie for this request. This is used to test
  // private browsing mode; the cookie should not be set if the image is
  // loaded anonymously.
  if (params.has("c")) {
    let expectedValue = parseInt(params.get("c"), 10);
    let actualValue = !request.hasHeader("Cookie") ? 0 :
                      parseInt(request.getHeader("Cookie")
                                      .replace(/^counter=(\d+)/, "$1"), 10);
    if (actualValue != expectedValue) {
      response.setStatusLine(request.httpVersion, 400, "Wrong counter value");
      return;
    }
    response.setHeader("Set-Cookie", `counter=${expectedValue + 1}`, false);
  }

  // Wait to send the image if a timeout is given.
  let timeout = parseInt(params.get("t"), 10);
  if (timeout > 0) {
    response.processAsync();
    setTimeout(() => {
      respond(params, request, response);
      response.finish();
    }, timeout * 1000);
    return;
  }

  respond(params, request, response);
}

function parseQueryString(queryString) {
  return queryString.split("&").reduce((params, param) => {
    let [key, value] = param.split("=", 2);
    params.set(key, value);
    return params;
  }, new Map());
}

function respond(params, request, response) {
  if (params.has("s")) {
    let statusCode = parseInt(params.get("s"), 10);
    response.setStatusLine(request.httpVersion, statusCode, "Custom status");
    return;
  }
  var filename = params.get("f");
  writeFile(filename, response);
}

function writeFile(name, response) {
  var file = new LocalFile(getState("__LOCATION__")).parent;
  file.append(name);

  let mimeType = Cc["@mozilla.org/uriloader/external-helper-app-service;1"]
                   .getService(Ci.nsIMIMEService)
                   .getTypeFromFile(file);

  let fileStream = new FileInputStream(file, 1, 0, false);
  let binaryStream = new BinaryInputStream(fileStream);

  response.setHeader("Content-Type", mimeType, false);
  response.bodyOutputStream.writeFrom(binaryStream, binaryStream.available());

  binaryStream.close();
  fileStream.close();
}
