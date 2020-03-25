function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);

  const maxBytesPerCookie = 4096;
  const maxBytesPerCookiePath = 1024;

  aResponse.setHeader("Set-Cookie", "a=" + Array(maxBytesPerCookie + 1).join('x'), true);
  aResponse.setHeader("Set-Cookie", "b=c; path=/" + Array(maxBytesPerCookiePath + 1).join('x'), true);
}
