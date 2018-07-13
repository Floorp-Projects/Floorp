function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);

  if (aRequest.queryString.includes("result")) {
    aResponse.write(getState("hints") || 0);
    setState("hints", "0");
    return;
  }

  if (aRequest.hasHeader('Cookie')) {
    let hints = parseInt(getState("hints") || 0) + 1;
    setState("hints", hints.toString());
  }

  aResponse.setHeader("Set-Cookie", "foopy=1");
  aResponse.setHeader("Content-Type", "text/javascript", false);
  aResponse.write("42;");
}
