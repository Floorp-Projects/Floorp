// Test that asking for a livemark in a annotationChanged notification works.
add_task(async function() {
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
      QueryInterface: ChromeUtils.generateQI([
        Ci.nsIAnnotationObserver
      ]),
    };
    PlacesUtils.annotations.addObserver(annoObserver);
  });


  let livemark = await PlacesUtils.livemarks.addLivemark(
    { title: "livemark title",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      index: PlacesUtils.bookmarks.DEFAULT_INDEX,
      siteURI: uri("http://example.com/"),
      feedURI: uri("http://example.com/rdf")
    });

  await annoPromise;

  livemark = await PlacesUtils.livemarks.getLivemark({ guid: livemark.guid });
  Assert.ok(livemark);
  await PlacesUtils.livemarks.removeLivemark({ guid: livemark.guid });
});
