function handleRequest(request, response)
{
  if (request.queryString == "throw")
    undefined[5];
  response.setHeader("X-Test-Status", "PASS", false);
}
