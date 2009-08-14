/**
 * Server-side javascript CGI for serving html5lib tokenizer test data.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;

/**
 * Read the contents of a file and return it as a string.
 */
function getFileContents(aFile) {
    const PR_RDONLY = 0x01;
    var fileStream = Cc["@mozilla.org/network/file-input-stream;1"]
                        .createInstance(Ci.nsIFileInputStream);
    fileStream.init(aFile, PR_RDONLY, 0400,
                  Ci.nsIFileInputStream.CLOSE_ON_EOF);
    var inputStream = Cc["@mozilla.org/scriptableinputstream;1"]
                        .createInstance(Ci.nsIScriptableInputStream);
    inputStream.init(fileStream);
    var data = "";
    do {
        var str = inputStream.read(inputStream.available());
        data += str;
    } while(str.length > 0);

    return data;
}

/**
 * Handle the HTTP request by writing an appropriate response.
 */
function handleRequest(request, response)
{
  // The query string of the request is expected to be in the format
  // xx&filename.test, where xx is an integer representing the index
  // of the test in filename.test to return.
  var index = parseInt(request.queryString, 10);
  var filename =
    request.queryString.substring(request.queryString.indexOf("&") + 1);

  var file = Components.classes["@mozilla.org/file/directory_service;1"]
             .getService(Components.interfaces.nsIProperties)
             .get("CurWorkD", Components.interfaces.nsIFile);

  file.append("tests");
  file.append("parser");
  file.append("htmlparser");
  file.append("tests");
  file.append("mochitest");

  file.append(filename);

  var fileContent = getFileContents(file);

  response.setHeader("Content-Type", "text/html; charset=utf-8");

  var nativeJSON = Components.classes["@mozilla.org/dom/json;1"]
                   .createInstance(Components.interfaces.nsIJSON);
  var tests =
    nativeJSON.decode(fileContent.substring(fileContent.indexOf("{")));

  // If the test case doesn't include anything that looks like
  // a doctype, prepend a standard doctype and header to the test
  // data.
  var bodystr = tests["tests"][(index - 1)]["input"];
  if (bodystr.toLowerCase().indexOf("<!doc") == -1) {
    response.write("<!DOCTYPE html><html><head><body>");
  }

  var bos = Components.classes["@mozilla.org/binaryoutputstream;1"]
      .createInstance(Components.interfaces.nsIBinaryOutputStream);
  bos.setOutputStream(response.bodyOutputStream);

  var body = [];
  for (var i = 0; i < bodystr.length; i++) {
    var charcode = bodystr.charCodeAt(i);
    // For unicode character codes greater than 65,536, two bytes
    // are used in the source string to represent the character.  Combine
    // these into a single decimal integer using the algorithm from
    // https://developer.mozilla.org/en/Core_JavaScript_1.5_Reference/
    // Global_Objects/String/charCodeAt
    if (0xD800 <= charcode && charcode <= 0xDBFF) 
    {
      var hi = charcode;
      var lo = bodystr.charCodeAt(++i);
      charcode = ((hi - 0xD800) * 0x400) + (lo - 0xDC00) + 0x10000;
    }
    // For unicode character codes greater than 128, pump out a
    // 2-, 3-, or 4-byte utf-8 sequence.
    if (charcode < 128) {
      body.push(charcode);
    }
    else if (charcode >=128 && charcode <=2047) {
      body.push(192 + Math.floor(charcode / 64));
      body.push(128 + (charcode % 64));
    } else if (charcode >=2048 && charcode <=65535) {
      body.push(224 + Math.floor(charcode / 4096));
      body.push(128 + (Math.floor(charcode / 64) % 64));
      body.push(128 + (charcode % 64));
    } else {
      body.push(240 + Math.floor(charcode / 262144));
      body.push(128 + (Math.floor(charcode / 4096) % 64));
      body.push(128 + (Math.floor(charcode / 64) % 64));
      body.push(128 + (charcode % 64));
    }
  }

  bos.writeByteArray(body, body.length);

}
