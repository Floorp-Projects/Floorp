function handleRequest(aRequest, aResponse) {
  if (aRequest.hasHeader("Origin")) {
    let origin = aRequest.getHeader("Origin");
    aResponse.setHeader("Access-Control-Allow-Origin", origin);
    aResponse.setHeader("Access-Control-Allow-Credentials", "true");
  }

  var params = new URLSearchParams(aRequest.queryString);
  if (params.has("redirect")) {
    aResponse.setHeader("Location", params.get("redirect"));
    aResponse.setStatusLine(aRequest.httpVersion, 302);
  } else {
    aResponse.setStatusLine(aRequest.httpVersion, 200);
  }

  if (!params.has("nocookie")) {
    aResponse.setHeader("Set-Cookie", "a=1; SameSite=None; Secure", true);
    aResponse.setHeader(
      "Set-Cookie",
      "b=2; Partitioned; SameSite=None; Secure",
      true
    );
  }
}
