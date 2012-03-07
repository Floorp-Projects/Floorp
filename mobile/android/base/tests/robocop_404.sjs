/**
 * Used with testThumbnails.
 * On the first visit, the page is green.
 * On subsequent visits, the page is red.
 */

function handleRequest(request, response) {
  let type = request.queryString.match(/^type=(.*)$/)[1];
  let state = "thumbnails." + type;
  let color = "#0f0";
  let status = 200;

  if (getState(state)) {
    color = "#f00";
    if (type == "do404")
      status = 404;
  } else {
    setState(state, "1");
  }

  response.setStatusLine(request.httpVersion, status, null);
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.write('<html>');
  response.write('<head><title>' + type + '</title></head>');
  response.write('<body style="background-color: ' + color + '"></body>');
  response.write('</html>');
}
