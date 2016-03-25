function handleRequest(request, response)
{
  response.setStatusLine("1.0", 200, "OK");
  response.setHeader("Content-Type", "text/plain; charset=utf-8", false);
  response.write("Ciao");
}
