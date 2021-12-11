function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);
  aResponse.setHeader("Clear-Site-Data", '"*"');
  aResponse.setHeader("Content-Type", "text/plain");
  aResponse.write("Clear-Site-Data");
}
