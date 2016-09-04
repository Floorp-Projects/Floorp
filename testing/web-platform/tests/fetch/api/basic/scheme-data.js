if (this.document === undefined) {
  importScripts("/resources/testharness.js");
  importScripts("../resources/utils.js");
}

function checkFetchResponse(url, data, mime, method) {
  var cut = (url.length >= 40) ? "[...]" : "";
  desc = "Fetching " + (method ? "[" + method + "] " : "") + url.substring(0, 40) + cut + " is OK";
  promise_test(function(test) {
    return fetch(url, {"method": method || "GET"}).then(function(resp) {
      assert_equals(resp.status, 200, "HTTP status is 200");
      assert_equals(resp.type, "basic", "response type is basic");
      assert_equals(resp.headers.get("Content-Type"), mime, "Content-Type is " + resp.headers.get("Content-Type"));
     return resp.text();
    }).then(function(body) {
        assert_equals(body, data, "Response's body is correct");
    });
  }, desc);
}

checkFetchResponse("data:,response%27s%20body", "response's body", "text/plain;charset=US-ASCII");
checkFetchResponse("data:text/plain;base64,cmVzcG9uc2UncyBib2R5", "response's body", "text/plain");
checkFetchResponse("data:image/png;base64,cmVzcG9uc2UncyBib2R5",
                   "response's body",
                   "image/png");
checkFetchResponse("data:,response%27s%20body", "response's body", "text/plain;charset=US-ASCII", "POST");
checkFetchResponse("data:,response%27s%20body", "response's body", "text/plain;charset=US-ASCII", "HEAD");

function checkKoUrl(url, method, desc) {
  var cut = (url.length >= 40) ? "[...]" : "";
  desc = "Fetching [" + method + "] " + url.substring(0, 45) + cut + " is KO"
  promise_test(function(test) {
    return promise_rejects(test, new TypeError(), fetch(url, {"method": method}));
  }, desc);
}

checkKoUrl("data:notAdataUrl.com", "GET");

done();
