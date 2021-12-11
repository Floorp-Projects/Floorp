if (this.document === undefined) {
  importScripts("/common/utils.js");
  importScripts("/resources/testharness.js");
  importScripts("/fetch/api/resources/utils.js");
  importScripts("/common/get-host-info.sub.js");
}

function testReferrerAfterRedirection(desc, redirectUrl, redirectLocation, referrerPolicy, redirectReferrerPolicy, expectedReferrer) {
  var url = redirectUrl;
  var urlParameters = "?location=" + encodeURIComponent(redirectLocation);

  if (redirectReferrerPolicy)
    urlParameters += "&redirect_referrerpolicy=" + redirectReferrerPolicy;

  var requestInit = {"redirect": "follow", "referrerPolicy": referrerPolicy};

    promise_test(function(test) {
      return fetch(url + urlParameters, requestInit).then(function(response) {
        assert_equals(response.status, 200, "Inspect header response's status is 200");
        assert_equals(response.headers.get("x-request-referer"), expectedReferrer ? expectedReferrer : null, "Check referrer header");
      });
    }, desc);
}

var referrerOrigin = get_host_info().HTTPS_ORIGIN + "/";
var referrerUrl = location.href;

var RESOURCES_DIR = "/fetch/api/resources/";
var redirectUrl = RESOURCES_DIR + "redirect.py";
var locationUrl = get_host_info().HTTPS_ORIGIN  + RESOURCES_DIR + "inspect-headers.py?headers=referer";
var httpLocationUrl =  get_host_info().HTTP_REMOTE_ORIGIN + RESOURCES_DIR + "inspect-headers.py?cors&headers=referer";

testReferrerAfterRedirection("Downgrade, empty init, unsafe-url redirect header ", redirectUrl, httpLocationUrl, "", "unsafe-url", referrerUrl);
testReferrerAfterRedirection("Downgrade, empty init, no-referrer-when-downgrade redirect header ", redirectUrl, httpLocationUrl, "", "no-referrer-when-downgrade", null);
testReferrerAfterRedirection("Downgrade, empty init, same-origin redirect header ", redirectUrl, httpLocationUrl, "", "same-origin", null);
testReferrerAfterRedirection("Downgrade, empty init, origin redirect header ", redirectUrl, httpLocationUrl, "", "origin", referrerOrigin);
testReferrerAfterRedirection("Downgrade, empty init, origin-when-cross-origin redirect header ", redirectUrl, httpLocationUrl, "", "origin-when-cross-origin", referrerOrigin);
testReferrerAfterRedirection("Downgrade, empty init, no-referrer redirect header ", redirectUrl, httpLocationUrl, "", "no-referrer", null);
testReferrerAfterRedirection("Downgrade, empty init, strict-origin redirect header ", redirectUrl, httpLocationUrl, "", "strict-origin", null);
testReferrerAfterRedirection("Downgrade, empty init, strict-origin-when-cross-origin redirect header ", redirectUrl, httpLocationUrl, "", "strict-origin-when-cross-origin", null);

testReferrerAfterRedirection("Downgrade, empty redirect header, unsafe-url init ", redirectUrl, httpLocationUrl, "unsafe-url", "", referrerUrl);
testReferrerAfterRedirection("Downgrade, empty redirect header, no-referrer-when-downgrade init ", redirectUrl, httpLocationUrl, "no-referrer-when-downgrade", "", null);
testReferrerAfterRedirection("Downgrade, empty redirect header, same-origin init ", redirectUrl, httpLocationUrl, "same-origin", "", null);
testReferrerAfterRedirection("Downgrade, empty redirect header, origin init ", redirectUrl, httpLocationUrl, "origin", "", referrerOrigin);
testReferrerAfterRedirection("Downgrade, empty redirect header, origin-when-cross-origin init ", redirectUrl, httpLocationUrl, "origin-when-cross-origin", "", referrerOrigin);
testReferrerAfterRedirection("Downgrade, empty redirect header, no-referrer init ", redirectUrl, httpLocationUrl, "no-referrer", "", null);
testReferrerAfterRedirection("Downgrade, empty redirect header, strict-origin init ", redirectUrl, httpLocationUrl, "strict-origin", "", null);
testReferrerAfterRedirection("Downgrade, empty redirect header, strict-origin-when-cross-origin init ", redirectUrl, httpLocationUrl, "strict-origin-when-cross-origin", "", null);


