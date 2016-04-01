function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  if (request.method == "POST") {
    setState(query["id"], "ping");
  } else {
    var value = getState(query["id"]);
    response.setHeader("Content-Type", "text/plain", false);
    response.write(value);
  }
}
