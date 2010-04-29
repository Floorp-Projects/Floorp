// Simple script redirects to the query part of the uri if the cookie "xpinstall"
// has the value "true", otherwise gives a 500 error.

function handleRequest(request, response)
{
  let cookie = null;
  if (request.hasHeader("Cookie")) {
    let cookies = request.getHeader("Cookie").split(";");
    for (let i = 0; i < cookies.length; i++) {
      if (cookies[i].substring(0, 10) == "xpinstall=")
        cookie = cookies[i].substring(10);
    }
  }

  if (cookie == "true") {
    response.setStatusLine(request.httpVersion, 302, "Found");
    response.setHeader("Location", request.queryString);
    response.write("See " + request.queryString);
  }
  else {
    response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
    response.write("Invalid request");
  }
}
