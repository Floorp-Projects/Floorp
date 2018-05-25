/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_brokenFolderShortcut() {
  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [{
      url: "http://1.moz.org/",
      title: "Bookmark 1",
    }, {
      url: "place:parent=1234",
      title: "Shortcut 1",
    }, {
      url: "place:parent=-1",
      title: "Shortcut 2",
    }, {
      url: "http://2.moz.org/",
      title: "Bookmark 2",
    }]
  });

  // Add also a simple visit.
  await PlacesTestUtils.addVisits(uri(("http://3.moz.org/")));

  // Query containing a broken folder shortcuts among results.
  let query = PlacesUtils.history.getNewQuery();
  query.setParents([PlacesUtils.bookmarks.unfiledGuid], 1);
  let options = PlacesUtils.history.getNewQueryOptions();
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  Assert.equal(root.childCount, 4);

  let shortcut = root.getChild(1);
  Assert.equal(shortcut.uri, "place:parent=1234");
  PlacesUtils.asContainer(shortcut);
  shortcut.containerOpen = true;
  Assert.equal(shortcut.childCount, 0);
  shortcut.containerOpen = false;
  // Remove the broken shortcut while the containing result is open.
  await PlacesUtils.bookmarks.remove(bookmarks[1]);
  Assert.equal(root.childCount, 3);

  shortcut = root.getChild(1);
  Assert.equal(shortcut.uri, "place:parent=-1");
  PlacesUtils.asContainer(shortcut);
  shortcut.containerOpen = true;
  Assert.equal(shortcut.childCount, 0);
  shortcut.containerOpen = false;
  // Remove the broken shortcut while the containing result is open.
  await PlacesUtils.bookmarks.remove(bookmarks[2]);
  Assert.equal(root.childCount, 2);

  root.containerOpen = false;

  // Broken folder shortcut as root node.
  query = PlacesUtils.history.getNewQuery();
  query.setParents([1234], 1);
  options = PlacesUtils.history.getNewQueryOptions();
  root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 0);
  root.containerOpen = false;

  // Broken folder shortcut as root node with folder=-1.
  query = PlacesUtils.history.getNewQuery();
  query.setParents([-1], 1);
  options = PlacesUtils.history.getNewQueryOptions();
  root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 0);
  root.containerOpen = false;
});
