function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 307, "Temporary Redirect");
  let location = request.queryString;
  response.setHeader("Location", location, false);
  response.write("Hello world!");
}
