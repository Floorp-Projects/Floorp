function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);
  let query = new URLSearchParams(aRequest.queryString);

  if (query.has("Set-Cookie")) {
    for (let value of query.getAll("Set-Cookie")) {
      aResponse.setHeader("Set-Cookie", value, true);
    }
    return;
  }

  let cookieStr = "";
  if (aRequest.hasHeader("Cookie")) {
    cookieStr = aRequest.getHeader("Cookie");
  }
  aResponse.write(cookieStr);
}
