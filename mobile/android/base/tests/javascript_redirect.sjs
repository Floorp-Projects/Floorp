function handleRequest(request, response)
{
  let page = "<!DOCTYPE html><html><head><script>window.opener = null; location.replace('" + request.queryString + "')</script></head><body><p>Redirecting...</p></body></html>";

  response.setStatusLine("1.0", 200, "OK");
  response.setHeader("Content-Type", "text/html; charset=utf-8", false);
  response.write(page);
}
