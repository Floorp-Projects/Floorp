function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);

  var params = new URLSearchParams(aRequest.queryString);

  // Get Cookie header string.
  if (params.has("get")) {
    if (aRequest.hasHeader("Cookie")) {
      let cookie = aRequest.getHeader("Cookie");
      aResponse.write(cookie);
    }
    return;
  }

  // Set a partitioned and a unpartitioned cookie.
  if (params.has("set")) {
    aResponse.setHeader(
      "Set-Cookie",
      "cookie=partitioned; Partitioned; SameSite=None; Secure",
      true
    );
    aResponse.setHeader(
      "Set-Cookie",
      "cookie=unpartitioned; SameSite=None; Secure",
      true
    );
  }
}
