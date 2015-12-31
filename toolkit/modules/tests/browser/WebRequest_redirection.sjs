function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 302);
  aResponse.setHeader("Location", "./dummy_page.html");
}
