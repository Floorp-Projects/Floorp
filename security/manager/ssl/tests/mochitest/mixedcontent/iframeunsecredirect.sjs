function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 307, "Moved temporarly");
  response.setHeader("Location", "http://example.com/tests/security/manager/ssl/tests/mochitest/mixedcontent/iframe.html");
}
