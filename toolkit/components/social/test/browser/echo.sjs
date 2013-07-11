// A server-side JS test file for frameworker testing. It exists only to
// work-around a lack of data: URL support in the frameworker.

function handleRequest(request, response)
{
  // The query string is the javascript - we just write it back.
  response.setHeader("Content-Type", "application/javascript; charset=utf-8", false);
  response.write(unescape(request.queryString));
}
