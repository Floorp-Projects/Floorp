function handleRequest(request, response)
{
  if (request.queryString == "throw")
    throw "monkey wrench!";

  response.setHeader("Content-Type", "text/plain", false);
  response.write("PASS");
}
