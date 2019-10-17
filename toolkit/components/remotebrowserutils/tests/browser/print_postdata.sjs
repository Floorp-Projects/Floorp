const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/plain", false);
  if (request.method == "GET") {
    response.write(request.queryString);
  } else {
    var body = new BinaryInputStream(request.bodyInputStream);

    var avail;
    var bytes = [];

    while ((avail = body.available()) > 0)
      Array.prototype.push.apply(bytes, body.readByteArray(avail));

    var data = String.fromCharCode.apply(null, bytes);
    if (request.queryString) {
      data = data + "?" + request.queryString;
    }
    response.bodyOutputStream.write(data, data.length);
  }
}
