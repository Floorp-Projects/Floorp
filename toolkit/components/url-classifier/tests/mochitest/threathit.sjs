/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function handleRequest(request, response)
{
  Components.utils.importGlobalProperties(["URLSearchParams"]);
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
    return;
  } else if (action == "get") {
    let state = params.get("list") + params.get("type");
    responseBody = base64ToString(getState(state));
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.bodyOutputStream.write(responseBody,
                                    responseBody.length);
  } else if (action == "report") {
    let state = params.get("list") + "report";
    let requestUrl = request.scheme + "://" + request.host + ":" +
      request.port + request.path + "?" + request.queryString;
    setState(state, requestUrl);
  } else if (action == "getreport") {
    let state = params.get("list") + "report";
    responseBody = getState(state);
    response.setHeader("Content-Type", "text/plain", false);
    response.write(responseBody);
  }
}

var base64Pad = '=';
/* Convert Base64 data to a string */
var toBinaryTable = [
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1, 0,-1,-1,
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
];

function base64ToString(data) {
    var result = '';
    var leftbits = 0; // number of bits decoded, but yet to be appended
    var leftdata = 0; // bits decoded, but yet to be appended

    // Convert one by one.
    for (var i = 0; i < data.length; i++) {
        var c = toBinaryTable[data.charCodeAt(i) & 0x7f];
        var padding = (data[i] == base64Pad);
        // Skip illegal characters and whitespace
        if (c == -1) continue;
        // Collect data into leftdata, update bitcount
        leftdata = (leftdata << 6) | c;
        leftbits += 6;

        // If we have 8 or more bits, append 8 bits to the result
        if (leftbits >= 8) {
            leftbits -= 8;
            // Append if not padding.
            if (!padding)
                result += String.fromCharCode((leftdata >> leftbits) & 0xff);
            leftdata &= (1 << leftbits) - 1;
        }
    }

    // If there are any bits left, the base64 string was corrupted
    if (leftbits)
        throw Components.Exception('Corrupted base64 string');

    return result;
}
