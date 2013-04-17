function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 307, "Moved temporarly");
  response.setHeader("Location", "https://example.com/tests/security/manager/ssl/tests/mochitest/mixedcontent/moonsurface.jpg");
}
