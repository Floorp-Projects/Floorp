const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var idx = val.indexOf('=');
    query[val.slice(0, idx)] = unescape(val.slice(idx + 1));
  });

  var responseBody;

  // Store fullhash in the server side.
  if ("list" in query && "fullhash" in query) {
    // In the server side we will store:
    // 1. All the full hashes for a given list
    // 2. All the lists we have right now
    // data is separate by '\n'
    let list = query["list"];
    let hashes = getState(list);

    let hash = base64ToString(query["fullhash"]);
    hashes += hash + "\n";
    setState(list, hashes);

    let lists = getState("lists");
    if (lists.indexOf(list) == -1) {
      lists += list + "\n";
      setState("lists", lists);
    }

    return;
  // gethash count return how many gethash request received.
  // This is used by client to know if a gethash request is triggered by gecko
  } else if ("gethashcount" == request.queryString) {
    var counter = getState("counter");
    responseBody = counter == "" ? "0" : counter;
  } else {
    var body = new BinaryInputStream(request.bodyInputStream);
    var avail;
    var bytes = [];

    while ((avail = body.available()) > 0) {
      Array.prototype.push.apply(bytes, body.readByteArray(avail));
    }

    var counter = getState("counter");
    counter = counter == "" ? "1" : (parseInt(counter) + 1).toString();
    setState("counter", counter);

    responseBody = parseV2Request(bytes);
  }

  response.setHeader("Content-Type", "text/plain", false);
  response.write(responseBody);

}

function parseV2Request(bytes) {
  var request = String.fromCharCode.apply(this, bytes);
  var [HEADER, PREFIXES] = request.split("\n");
  var [PREFIXSIZE, LENGTH] = HEADER.split(":").map(val => {
      return parseInt(val);
    });

  var ret = "";
  for(var start = 0; start < LENGTH; start += PREFIXSIZE) {
    getState("lists").split("\n").forEach(function(list) {
      var completions = getState(list).split("\n");

      for (var completion of completions) {
        if (completion.indexOf(PREFIXES.substr(start, PREFIXSIZE)) == 0) {
          ret += list + ":" + "1" + ":" + "32" + "\n";
          ret += completion;
        }
      }
    });
  }

  return ret;
}

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
