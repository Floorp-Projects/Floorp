// Simple script redirects takes the query part of te request and splits it on
// the | character. Anything before is included as the X-Target-Digest header
// the latter part is used as the url to redirect to

function handleRequest(request, response)
{
  let pos = request.queryString.indexOf("|");
  let header = request.queryString.substring(0, pos);
  let url = request.queryString.substring(pos + 1);

  response.setStatusLine(request.httpVersion, 302, "Found");
  response.setHeader("X-Target-Digest", header);
  response.setHeader("Location", url);
  response.write("See " + url);
}
