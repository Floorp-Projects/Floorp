const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function readStream(inputStream) {
  let available = 0;
  let result = [];
  while ((available = inputStream.available()) > 0) {
    result.push(inputStream.readBytes(available));
  }

  return result.join('');
}

function handleRequest(request, response)
{
  if (request.method != "POST") {
    response.setStatusLine(request.httpVersion, 405, "Method Not Allowed");
    return;
  }

  if (request.hasHeader("Authorization")) {
    let data = "";
    try {
      data = readStream(new BinaryInputStream(request.bodyInputStream));
    } catch (e) {
      data = `${e}`;
    }
    response.bodyOutputStream.write(data, data.length);
    return;
  }

  response.setStatusLine(request.httpVersion, 401, "Unauthorized");
  response.setHeader("WWW-Authenticate", `basic realm="test"`, true);
}
