const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function handleRequest(request, response)
{
  var body = "";
  var bodyStream = new BinaryInputStream(request.bodyInputStream);
  var bytes = [], avail = 0;
  while ((avail = bodyStream.available()) > 0)
   body += String.fromCharCode.apply(String, bodyStream.readByteArray(avail));

  response.setHeader("Content-Type", "text/html", false);
  response.write(body);
}

