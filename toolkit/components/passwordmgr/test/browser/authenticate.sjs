function handleRequest(request, response)
{
  var match;
  var requestAuth = true;

  // Allow the caller to drive how authentication is processed via the query.
  // Eg, http://localhost:8888/authenticate.sjs?user=foo&realm=bar
  // The extra ? allows the user/pass/realm checks to succeed if the name is
  // at the beginning of the query string.
  var query = "?" + request.queryString;

  var expected_user = "test", expected_pass = "testpass", realm = "mochitest";

  // Look for an authentication header, if any, in the request.
  //
  // EG: Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==
  //
  // This test only supports Basic auth. The value sent by the client is
  // "username:password", obscured with base64 encoding.

  var actual_user = "", actual_pass = "", authHeader, authPresent = false;
  if (request.hasHeader("Authorization")) {
    authPresent = true;
    authHeader = request.getHeader("Authorization");
    match = /Basic (.+)/.exec(authHeader);
    if (match.length != 2)
        throw "Couldn't parse auth header: " + authHeader;

    var userpass = base64ToString(match[1]); // no atob() :-(
    match = /(.*):(.*)/.exec(userpass);
    if (match.length != 3)
        throw "Couldn't decode auth header: " + userpass;
    actual_user = match[1];
    actual_pass = match[2];
  }

  // Don't request authentication if the credentials we got were what we
  // expected.
  if (expected_user == actual_user &&
    expected_pass == actual_pass) {
    requestAuth = false;
  }

  if (requestAuth) {
    response.setStatusLine("1.0", 401, "Authentication required");
    response.setHeader("WWW-Authenticate", "basic realm=\"" + realm + "\"", true);
  } else {
    response.setStatusLine("1.0", 200, "OK");
  }

  response.setHeader("Content-Type", "application/xhtml+xml", false);
  response.write("<html xmlns='http://www.w3.org/1999/xhtml'>");
  response.write("<p>Login: <span id='ok'>" + (requestAuth ? "FAIL" : "PASS") + "</span></p>\n");
  response.write("<p>Auth: <span id='auth'>" + authHeader + "</span></p>\n");
  response.write("<p>User: <span id='user'>" + actual_user + "</span></p>\n");
  response.write("<p>Pass: <span id='pass'>" + actual_pass + "</span></p>\n");
  response.write("</html>");
}


// base64 decoder
//
// Yoinked from extensions/xml-rpc/src/nsXmlRpcClient.js because btoa()
// doesn't seem to exist. :-(
/* Convert Base64 data to a string */
const toBinaryTable = [
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1, 0,-1,-1,
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
];
const base64Pad = '=';

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
