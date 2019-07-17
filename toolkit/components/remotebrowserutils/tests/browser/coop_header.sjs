function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 200, "OK");

  let coop = request.queryString.replace(/\./g, '');
  if (coop.length > 0) {
    response.setHeader("Cross-Origin-Opener-Policy", unescape(coop), false);
  }

  response.setHeader("Content-Type", "text/html; charset=utf-8", false);

  response.write("<!DOCTYPE html><html><body><p>Hello world</p></body></html>");
}
