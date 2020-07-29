/*
 * Redirect handler specifically for the needs of:
 * Bug 1194052 - Append Principal to RedirectChain within LoadInfo before the channel is succesfully openend
 */

function createIframeContent(aQuery) {

  var content =`
  <!DOCTYPE HTML>
  <html>
  <head><meta charset="utf-8">
  <title>Bug 1194052 - LoadInfo redirect chain subtest</title>
  </head>
  <body>
  <script type="text/javascript">
    var myXHR = new XMLHttpRequest();
    myXHR.open("GET", "http://example.com/tests/netwerk/test/mochitests/file_loadinfo_redirectchain.sjs?` + aQuery + `");
    myXHR.onload = function() {
    var loadinfo = SpecialPowers.wrap(myXHR).channel.loadInfo;
    var redirectChain = loadinfo.redirectChain;
    var redirectChainIncludingInternalRedirects = loadinfo.redirectChainIncludingInternalRedirects;
    var resultOBJ = { redirectChain : [], redirectChainIncludingInternalRedirects : [] };
    for (var i = 0; i < redirectChain.length; i++) {
      resultOBJ.redirectChain.push(redirectChain[i].principal.URI.spec);
    }
    for (var i = 0; i < redirectChainIncludingInternalRedirects.length; i++) {
      resultOBJ.redirectChainIncludingInternalRedirects.push(redirectChainIncludingInternalRedirects[i].principal.URI.spec);
    }
    var loadinfoJSON = JSON.stringify(resultOBJ);
    window.parent.postMessage({ loadinfo: loadinfoJSON }, "*");
  }
  myXHR.onerror = function() {
    var resultOBJ = { redirectChain : [], redirectChainIncludingInternalRedirects : [] };
    var loadinfoJSON = JSON.stringify(resultOBJ);
    window.parent.postMessage({ loadinfo: loadinfoJSON }, "*");
  }
  myXHR.send();
  </script>
  </body>
  </html>`;

  return content;
}

function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);
  var queryString = request.queryString;

  if (queryString == "iframe-redir-https-2" ||
      queryString == "iframe-redir-err-2") {
    var query = queryString.replace("iframe-", "");
    // send upgrade-insecure-requests CSP header
    response.setHeader("Content-Type", "text/html", false);
    response.setHeader("Content-Security-Policy", "upgrade-insecure-requests", false);
    response.write(createIframeContent(query));
    return;
  }

  // at the end of the redirectchain we return some text
  // for sanity checking
  if (queryString == "redir-0" ||
      queryString == "redir-https-0") {
    response.setHeader("Content-Type", "text/html", false);
    response.write("checking redirectchain");
    return;
  }

  // special case redir-err-1 and return an error to trigger the fallback
  if (queryString == "redir-err-1") {
    response.setStatusLine("1.1", 404, "Bad request");
    return;
  }

  // must be a redirect
  var newLoaction = "";
  switch (queryString) {
    case "redir-err-2":
      newLocation =
        "http://example.com/tests/netwerk/test/mochitests/file_loadinfo_redirectchain.sjs?redir-err-1";
      break;

    case "redir-https-2":
      newLocation =
        "http://example.com/tests/netwerk/test/mochitests/file_loadinfo_redirectchain.sjs?redir-https-1";
      break;

    case "redir-https-1":
      newLocation =
        "http://example.com/tests/netwerk/test/mochitests/file_loadinfo_redirectchain.sjs?redir-https-0";
      break;

    case "redir-2":
      newLocation =
        "http://mochi.test:8888/tests/netwerk/test/mochitests/file_loadinfo_redirectchain.sjs?redir-1";
      break;

    case "redir-1":
      newLocation =
        "http://mochi.test:8888/tests/netwerk/test/mochitests/file_loadinfo_redirectchain.sjs?redir-0";
      break;
  }

  response.setStatusLine("1.1", 302, "Found");
  response.setHeader("Location", newLocation, false);
}
