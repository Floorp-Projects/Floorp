function handleRequest(request, response) {
  var count = parseInt(getState("count"));
  if (!count) {
    count = 0;
  }

  if (count == 0) {
    response.setStatusLine(request.httpVersion, "200", "OK");
    response.setHeader("Content-Type", "text/html", false);
    response.write("<html><body>Hello world!</body></html>");
    setState("count", "1");
    return;
  }

  response.setStatusLine(request.httpVersion, "304", "Not Modified");
  response.setHeader("Clear-Site-Data", '"storage"');
}
