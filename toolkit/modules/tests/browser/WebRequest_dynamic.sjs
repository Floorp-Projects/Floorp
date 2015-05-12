function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);
  if (aRequest.hasHeader('Cookie')) {
    let value = aRequest.getHeader("Cookie");
    if (value == "blinky=1") {
      aResponse.setHeader("Set-Cookie", "dinky=1");
    }
    aResponse.write("cookie-present");
  } else {
    aResponse.setHeader("Set-Cookie", "foopy=1");
    aResponse.write("cookie-not-present");
  }
}
