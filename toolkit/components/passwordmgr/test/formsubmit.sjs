function handleRequest(request, response)
{
  try {
    reallyHandleRequest(request, response);
  } catch (e) {
    response.setStatusLine("1.0", 200, "AlmostOK");
    response.write("Error handling request: " + e);
  }
}


function reallyHandleRequest(request, response) {
  var match;
  var requestAuth = true;

  // XXX I bet this doesn't work for POST requests.
  var query = request.queryString;

  var user = null, pass = null;
  // user=xxx
  match = /user=([^&]*)/.exec(query);
  if (match)
    user = match[1];

  // pass=xxx
  match = /pass=([^&]*)/.exec(query);
  if (match)
    pass = match[1];

  response.setStatusLine("1.0", 200, "OK");

  response.setHeader("Content-Type", "application/xhtml+xml", false);
  response.write("<html xmlns='http://www.w3.org/1999/xhtml'>");
  response.write("<p>User: <span id='user'>" + user + "</span></p>\n");
  response.write("<p>Pass: <span id='pass'>" + pass + "</span></p>\n");
  response.write("</html>");
}
