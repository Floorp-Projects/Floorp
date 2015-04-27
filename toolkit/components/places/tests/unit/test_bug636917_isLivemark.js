// Test that asking for a livemark in a annotationChanged notification works.
add_task(function* () {
  let annoPromise = new Promise(resolve => {
    let annoObserver = {
      onItemAnnotationSet(id, name) {
        if (name == PlacesUtils.LMANNO_FEEDURI) {
          PlacesUtils.annotations.removeObserver(this);
          resolve();
        }
      },
      onItemAnnotationRemoved() {},
      onPageAnnotationSet() {},
      onPageAnnotationRemoved() {},
      QueryInterface: XPCOMUtils.generateQI([
        Ci.nsIAnnotationObserver
      ]),
    };
    PlacesUtils.annotations.addObserver(annoObserver, false);
  });


  let livemark = yield PlacesUtils.livemarks.addLivemark(
    { title: "livemark title"
    , parentGuid: PlacesUtils.bookmarks.unfiledGuid
    , index: PlacesUtils.bookmarks.DEFAULT_INDEX
    , siteURI: uri("http://example.com/")
    , feedURI: uri("http://example.com/rdf")
    });

  yield annoPromise;

  livemark = yield PlacesUtils.livemarks.getLivemark({ guid: livemark.guid });
  Assert.ok(livemark);
  yield PlacesUtils.livemarks.removeLivemark({ guid: livemark.guid });
});
