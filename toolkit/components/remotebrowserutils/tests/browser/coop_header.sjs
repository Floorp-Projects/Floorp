function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 200, "OK");

  let qs = request.queryString.replace(/\./g, '');

  let isDownloadPage = false;
  let isDownloadFile = false;
  if (qs.length > 0) {
    qs.split("&").forEach(param => {
      if (param === "downloadPage") {
        isDownloadPage = true;
      } else if (param === "downloadFile") {
        isDownloadFile = true;
      } else if (param.length > 0) {
        response.setHeader("Cross-Origin-Opener-Policy", unescape(param), false);
      }
    });
  }

  let downloadHTML = "";
  if (isDownloadPage) {
    [
     "no-coop", "same-site", "same-origin", "same-site%20unsafe-allow-outgoing",
     "same-origin%20unsafe-allow-outgoing"
    ].forEach(coop => {
      downloadHTML +=
        '<a href="https://example.com/browser/toolkit/components/remotebrowserutils/tests/browser/coop_header.sjs?downloadFile&' +
        (coop === "no-coop" ? "" : coop) + '" id="' + coop + '" download>' + unescape(coop) +  '</a> <br>';
    });
  }

  if (isDownloadFile) {
    response.setHeader("Content-Type", "application/octet-stream", false);
    response.write("BINARY_DATA");
  } else {
    response.setHeader("Content-Type", "text/html; charset=utf-8", false);
    response.write("<!DOCTYPE html><html><body><p>Hello world</p> " + downloadHTML + "</body></html>");
  }
}
