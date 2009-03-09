function handleRequest(request, response)
{
  response.setStatusLine("1.1", 302, "Found");
  response.setHeader("Location", "red.png", false);
  response.setHeader("Cache-Control", "no-cache", false);
}
