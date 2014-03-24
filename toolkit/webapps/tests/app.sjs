function getQuery(request) {
  let query = {};

  request.queryString.split('&').forEach(function(val) {
    let [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  return query;
}

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  let query = getQuery(request);

  if ("appreq" in query) {
    response.setHeader("Content-Type", "text/plain", false);
    response.write("Hello world!");

    setState("appreq", "done");

    return;
  }

  if ("testreq" in query) {
    response.setHeader("Content-Type", "text/plain", false);

    response.write(getState("appreq"));

    return;
  }
}
