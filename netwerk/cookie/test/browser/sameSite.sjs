function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);

  aResponse.setHeader("Set-Cookie", "a=1", true);
  aResponse.setHeader("Set-Cookie", "b=2; sameSite=none", true);
  aResponse.setHeader("Set-Cookie", "c=3; sameSite=batman", true);
}
