var manifest =
  "CACHE MANIFEST\n"

function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  if (query["update"]) {
    setState("update", "#" + Date.now() + "\n");
  }
  var update = getState("update");

  response.setStatusLine(request.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "text/cache-manifest");
  response.setHeader("Cache-Control", "no-cache");
  response.write(manifest);
  response.write(update ? update : "#default\n");
}
