function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 302);

  let query = aRequest.queryString;
  let locations =  query.split("|");
  let nextLocation = locations.shift();
  if (locations.length) {
    nextLocation += "?" + locations.join("|");
  }
  aResponse.setHeader("Location", nextLocation);
}
