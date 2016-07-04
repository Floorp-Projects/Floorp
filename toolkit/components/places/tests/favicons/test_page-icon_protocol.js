const ICON_DATA = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAAAAAA6fptVAAAACklEQVQI12NgAAAAAgAB4iG8MwAAAABJRU5ErkJggg==";
const TEST_URI = NetUtil.newURI("http://mozilla.org/");
const ICON_URI = NetUtil.newURI("http://mozilla.org/favicon.ico");

function fetchIconForSpec(spec) {
 return new Promise((resolve, reject) => {
    NetUtil.asyncFetch({
      uri: NetUtil.newURI("page-icon:" + TEST_URI.spec),
      loadUsingSystemPrincipal: true,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE
    }, (input, status, request) => {
       if (!Components.isSuccessCode(status)) {
        reject(new Error("unable to load icon"));
        return;
      }

      try {
        let data = NetUtil.readInputStreamToString(input, input.available());
        let contentType = request.QueryInterface(Ci.nsIChannel).contentType;
        input.close();
        resolve({ data, contentType });
      } catch (ex) {
        reject(ex);
      }
    });
  });
}

var gDefaultFavicon;
var gFavicon;

add_task(function* setup() {
  PlacesTestUtils.addVisits({ uri: TEST_URI });

  PlacesUtils.favicons.replaceFaviconDataFromDataURL(
    ICON_URI, ICON_DATA, (Date.now() + 8640000) * 1000,
    Services.scriptSecurityManager.getSystemPrincipal());

  yield new Promise(resolve => {
    PlacesUtils.favicons.setAndFetchFaviconForPage(
      TEST_URI, ICON_URI, false,
      PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
      resolve, Services.scriptSecurityManager.getSystemPrincipal());
  });

  gDefaultFavicon = yield fetchIconForSpec(PlacesUtils.favicons.defaultFavicon);
  gFavicon = yield fetchIconForSpec(ICON_DATA);
});

add_task(function* known_url() {
  let {data, contentType} = yield fetchIconForSpec(TEST_URI.spec);
  Assert.equal(contentType, gFavicon.contentType);
  Assert.ok(data == gFavicon.data, "Got the favicon data");
});

add_task(function* unknown_url() {
  let {data, contentType} = yield fetchIconForSpec("http://www.moz.org/");
  Assert.equal(contentType, gDefaultFavicon.contentType);
  Assert.ok(data == gDefaultFavicon.data, "Got the default favicon data");
});

add_task(function* invalid_url() {
  let {data, contentType} = yield fetchIconForSpec("test");
  Assert.equal(contentType, gDefaultFavicon.contentType);
  Assert.ok(data == gDefaultFavicon.data, "Got the default favicon data");
});
