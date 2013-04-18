function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 307, "Moved temporarly");
  response.setHeader("Location", "http://example.com/tests/security/ssl/mixedcontent/emptyimage.sjs");
}
