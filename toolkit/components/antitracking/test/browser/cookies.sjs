function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);
  let cookie = "";
  if (aRequest.hasHeader("Cookie")) {
    cookie = aRequest.getHeader("Cookie");
  }
  aResponse.write("cookie:" + cookie);

  if (aRequest.queryString) {
    aResponse.setHeader("Set-Cookie", "foopy=" + aRequest.queryString);
  }
}
