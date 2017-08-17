function handleRequest(request, response) {
  let page = "<!DOCTYPE html><html><body><p>HSTS page</p></body></html>";
  response.setStatusLine(request.httpVersion, "200", "OK");
  response.setHeader("Strict-Transport-Security", "max-age=60");
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Content-Length", page.length + "", false);
  response.write(page);
}
