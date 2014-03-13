/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that asking for isLivemark in a annotationChanged notification
// correctly returns true.
function run_test()
{
  do_test_pending();

  let annoObserver = {
    onItemAnnotationSet:
    function AO_onItemAnnotationSet(aItemId, aAnnotationName)
    {
      if (aAnnotationName == PlacesUtils.LMANNO_FEEDURI) {
        PlacesUtils.annotations.removeObserver(this);
        PlacesUtils.livemarks.getLivemark({ id: aItemId })
          .then(aLivemark => {
            PlacesUtils.bookmarks.removeItem(aItemId);
            do_test_finished();
          }, do_throw);
      }
    },
  
    onItemAnnotationRemoved: function () {},
    onPageAnnotationSet: function() {},
    onPageAnnotationRemoved: function() {},
    QueryInterface: XPCOMUtils.generateQI([
      Ci.nsIAnnotationObserver
    ]),
  }
  PlacesUtils.annotations.addObserver(annoObserver, false);
  PlacesUtils.livemarks.addLivemark(
    { title: "livemark title"
    , parentId: PlacesUtils.unfiledBookmarksFolderId
    , index: PlacesUtils.bookmarks.DEFAULT_INDEX
    , siteURI: uri("http://example.com/")
    , feedURI: uri("http://example.com/rdf")
    }
  ).then(null, do_throw);
}
