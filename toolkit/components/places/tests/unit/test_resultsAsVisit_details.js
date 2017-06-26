/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */

const {history} = PlacesUtils;

add_task(async function test_addVisitCheckFields() {
  let uri = NetUtil.newURI("http://test4.com/");
  await PlacesTestUtils.addVisits([
    { uri },
    { uri, referrer: uri },
    { uri, transition: history.TRANSITION_TYPED },
  ]);


  let options = history.getNewQueryOptions();
  let query = history.getNewQuery();

  query.uri = uri;


  // Check RESULTS_AS_VISIT node.
  options.resultType = options.RESULTS_AS_VISIT;

  let root = history.executeQuery(query, options).root;
  root.containerOpen = true;

  equal(root.childCount, 3);

  let child = root.getChild(0);
  equal(child.visitType, history.TRANSITION_LINK, "Visit type should be TRANSITION_LINK");
  equal(child.visitId, 1, "Visit ID should be 1");
  equal(child.fromVisitId, -1, "Should have no referrer visit ID");

  child = root.getChild(1);
  equal(child.visitType, history.TRANSITION_LINK, "Visit type should be TRANSITION_LINK");
  equal(child.visitId, 2, "Visit ID should be 2");
  equal(child.fromVisitId, 1, "First visit should be the referring visit");

  child = root.getChild(2);
  equal(child.visitType, history.TRANSITION_TYPED, "Visit type should be TRANSITION_TYPED");
  equal(child.visitId, 3, "Visit ID should be 3");
  equal(child.fromVisitId, -1, "Should have no referrer visit ID");

  root.containerOpen = false;


  // Check RESULTS_AS_URI node.
  options.resultType = options.RESULTS_AS_URI;

  root = history.executeQuery(query, options).root;
  root.containerOpen = true;

  equal(root.childCount, 1);

  child = root.getChild(0);
  equal(child.visitType, 0, "Visit type should be 0");
  equal(child.visitId, -1, "Visit ID should be -1");
  equal(child.fromVisitId, -1, "Referrer visit id should be -1");

  root.containerOpen = false;

  await PlacesTestUtils.clearHistory();
});

add_task(async function test_bookmarkFields() {
  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [{
      title: "test folder",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      children: [{
        title: "test title",
        url: "http://test4.com",
      }]
    }],
  });

  let folderId = await PlacesUtils.promiseItemId(bookmarks[0].guid);

  let root = PlacesUtils.getFolderContents(folderId).root;
  equal(root.childCount, 1);

  equal(root.visitType, 0, "Visit type should be 0");
  equal(root.visitId, -1, "Visit ID should be -1");
  equal(root.fromVisitId, -1, "Referrer visit id should be -1");

  let child = root.getChild(0);
  equal(child.visitType, 0, "Visit type should be 0");
  equal(child.visitId, -1, "Visit ID should be -1");
  equal(child.fromVisitId, -1, "Referrer visit id should be -1");

  root.containerOpen = false;

  await PlacesUtils.bookmarks.eraseEverything();
});
