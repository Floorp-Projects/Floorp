function handleRequest(aRequest, aResponse) {
 if (aRequest.queryString.includes("redirect")) {
   aResponse.setStatusLine(aRequest.httpVersion, 302);
   if (aRequest.queryString.includes("redirect-checkonly")) {
     aResponse.setHeader("Location", "server.sjs?checkonly");
   } else {
     aResponse.setHeader("Location", "server.sjs");
   }
   return;
 }
 aResponse.setStatusLine(aRequest.httpVersion, 200);
 if (aRequest.hasHeader('Cookie')) {
   aResponse.write("cookie-present");
 } else {
   if (!aRequest.queryString.includes("checkonly")) {
     aResponse.setHeader("Set-Cookie", "foopy=1");
   }
   aResponse.write("cookie-not-present");
 }
}
