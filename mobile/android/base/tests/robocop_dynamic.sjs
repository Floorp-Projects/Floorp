/**
 * Dynamically generated page whose title matches the given id.
 */

function handleRequest(request, response) {
  let id = request.queryString.match(/^id=(.*)$/)[1];
  let key = "dynamic." + id;

  response.setStatusLine(request.httpVersion, 200, null);
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.write('<html>');
  response.write('<head><title>' + id + '</title><meta charset="utf-8"></head>');
  response.write('<body>');
  response.write('<h1>' + id + '</h1>');
  response.write('</body>');
  response.write('</html>');
}
