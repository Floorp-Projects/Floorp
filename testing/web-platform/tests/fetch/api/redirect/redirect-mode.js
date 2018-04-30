if (this.document === undefined) {
  importScripts("/resources/testharness.js");
  importScripts("/common/get-host-info.sub.js")
}

var redirectUrl = get_host_info().HTTPS_ORIGIN + "/fetch/api/resources/redirect.py";
var redirectLocation = "top.txt";

function testRedirect(redirectStatus, redirectMode, corsMode) {
  var url = redirectUrl;
  var urlParameters = "?redirect_status=" + redirectStatus;
  urlParameters += "&location=" + encodeURIComponent(redirectLocation);

  var requestInit = {redirect: redirectMode, mode: corsMode};

  promise_test(function(test) {
    if (redirectMode === "error" || (corsMode === "no-cors" && redirectMode !== "follow"))
      return promise_rejects(test, new TypeError(), fetch(url + urlParameters, requestInit));
    if (redirectMode === "manual")
      return fetch(url + urlParameters, requestInit).then(function(resp) {
        assert_equals(resp.status, 0, "Response's status is 0");
        assert_equals(resp.type, "opaqueredirect", "Response's type is opaqueredirect");
        assert_equals(resp.statusText, "", "Response's statusText is \"\"");
        assert_equals(resp.url, url + urlParameters, "Response URL should be the original one");
      });
    if (redirectMode === "follow")
      return fetch(url + urlParameters, requestInit).then(function(resp) {
        assert_true(new URL(resp.url).pathname.endsWith(redirectLocation), "Response's url should be the redirected one");
        assert_equals(resp.status, 200, "Response's status is 200");
      });
    assert_unreached(redirectMode + " is no a valid redirect mode");
  }, "Redirect " + statusCode + " in " + redirectMode + " redirect and " + mode + " mode");
}

for (var statusCode of [301, 302, 303, 307, 308]) {
  for (var redirect of ["error", "manual", "follow"]) {
    for (var mode of ["cors", "no-cors"])
      testRedirect(statusCode, redirect, mode);
  }
}

done();
