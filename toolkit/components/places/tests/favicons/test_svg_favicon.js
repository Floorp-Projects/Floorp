const PAGEURI = NetUtil.newURI("http://deliciousbacon.com/");

add_task(function* () {
  // First, add a history entry or else Places can't save a favicon.
  yield PlacesTestUtils.addVisits({
    uri: PAGEURI,
    transition: TRANSITION_LINK,
    visitDate: Date.now() * 1000
  });

  yield new Promise(resolve => {
    function onSetComplete(aURI, aDataLen, aData, aMimeType, aWidth) {
      equal(aURI.spec, SMALLSVG_DATA_URI.spec, "setFavicon aURI check");
      equal(aDataLen, 263, "setFavicon aDataLen check");
      equal(aMimeType, "image/svg+xml", "setFavicon aMimeType check");
      dump(aWidth);
      resolve();
    }

    PlacesUtils.favicons.setAndFetchFaviconForPage(PAGEURI, SMALLSVG_DATA_URI,
                                                   false,
                                                   PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
                                                   onSetComplete,
                                                   Services.scriptSecurityManager.getSystemPrincipal());
  });

  let data = yield PlacesUtils.promiseFaviconData(PAGEURI.spec);
  equal(data.uri.spec, SMALLSVG_DATA_URI.spec, "getFavicon aURI check");
  equal(data.dataLen, 263, "getFavicon aDataLen check");
  equal(data.mimeType, "image/svg+xml", "getFavicon aMimeType check");
});

