function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 200, "OK");

  let coop = request.queryString;
  if (coop.length > 0) {
    response.setHeader("Cross-Origin", unescape(coop), false);
  }

  response.setHeader("Content-Type", "text/html; charset=utf-8", false);

  response.write(`<!DOCTYPE html><html><body><p>Hello world: ${coop}</p></body></html>`);
}
