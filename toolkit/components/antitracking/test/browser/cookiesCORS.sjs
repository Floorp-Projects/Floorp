function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);
  aResponse.setHeader("Access-Control-Allow-Origin", "http://example.net");
  aResponse.setHeader("Access-Control-Allow-Credentials", "true");

  if (aRequest.queryString) {
    aResponse.setHeader("Set-Cookie", "foopy=" + aRequest.queryString);
  }
}
