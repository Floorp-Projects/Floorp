function handleRequest(request, response) {
  var queryString = request.queryString;
  switch (queryString) {
    case "xhr1":
      response.setHeader("Set-Cookie", "xhr1=xhr_val1; path=/", false);
      break;
    case "xhr2":
      response.setHeader(
        "Set-Cookie",
        "xhr2=xhr_val2; path=/; HttpOnly",
        false
      );
      break;
  }
}
