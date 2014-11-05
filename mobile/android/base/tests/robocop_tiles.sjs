/**
 * Used with testDistribution.
 * Responds by simply echoing back the request data.
 */

const cc = Components.Constructor;
const BinaryInputStream = cc("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function handleRequest(request, response) {
  let bodyStream = new BinaryInputStream(request.bodyInputStream);
  let avail;
  let bytes = [];
  while ((avail = bodyStream.available()) > 0) {
    Array.prototype.push.apply(bytes, bodyStream.readByteArray(avail));
  }
  let data = String.fromCharCode.apply(null, bytes);

  // Including this header will cause Gecko to broadcast the Robocop:TilesResponse event.
  response.setHeader("X-Robocop", "true", false);

  response.setHeader("Content-Type", "application/json", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.write(data);
}
