const CC = Components.Constructor;
const BinaryInputStream = CC(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

function handleRequest(request, response) {
  var query = {};
  request.queryString.split("&").forEach(function (val) {
    var idx = val.indexOf("=");
    query[val.slice(0, idx)] = unescape(val.slice(idx + 1));
  });

  // Store fullhash in the server side.
  if ("list" in query && "fullhash" in query) {
    // In the server side we will store:
    // 1. All the full hashes for a given list
    // 2. All the lists we have right now
    // data is separate by '\n'
    let list = query.list;
    let hashes = getState(list);

    let hash = atob(query.fullhash);
    hashes += hash + "\n";
    setState(list, hashes);

    let lists = getState("lists");
    if (!lists.includes(list)) {
      lists += list + "\n";
      setState("lists", lists);
    }

    return;
  }

  var body = new BinaryInputStream(request.bodyInputStream);
  var avail;
  var bytes = [];

  while ((avail = body.available()) > 0) {
    Array.prototype.push.apply(bytes, body.readByteArray(avail));
  }

  var responseBody = parseV2Request(bytes);

  response.setHeader("Content-Type", "text/plain", false);
  response.write(responseBody);
}

function parseV2Request(bytes) {
  var table = String.fromCharCode.apply(this, bytes).slice(0, -2);

  var ret = "";
  getState("lists")
    .split("\n")
    .forEach(function (list) {
      if (list == table) {
        var completions = getState(list).split("\n");
        ret += "n:1000\n";
        ret += "i:" + list + "\n";
        ret += "a:1:32:" + 32 * (completions.length - 1) + "\n";

        for (var completion of completions) {
          ret += completion;
        }
      }
    });

  return ret;
}
