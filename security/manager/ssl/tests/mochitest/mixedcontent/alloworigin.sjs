function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Access-Control-Allow-Origin", "*");
  response.write("<html><body>hello!</body></html>");
}
