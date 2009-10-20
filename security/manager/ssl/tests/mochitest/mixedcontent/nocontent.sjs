function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 204, "No Content");
}
