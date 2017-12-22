
function handleRequest(request, response)
{
  var queryString = request.queryString;
  switch (queryString) {
    case "set_cookie":
      response.setHeader("Set-Cookie", "testXHR1=xhr_val1; path=/", false);
    break;
    case "modify_cookie":
      response.setHeader("Set-Cookie", "testXHR1=xhr_val2; path=/; HttpOnly", false);
    break;
  }
}
