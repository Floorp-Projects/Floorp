/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_explicit_weakupload() {
  let buf = await openMirror("weakupload");

  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      guid: "mozBmk______",
      url: "https://mozilla.org",
      title: "Mozilla",
      tags: ["moz", "dot", "org"],
    }],
  });
  await buf.store(shuffle([{
    id: "menu",
    type: "folder",
    children: ["mozBmk______"],
  }, {
    id: "mozBmk______",
    type: "bookmark",
    title: "Mozilla",
    bmkUri: "https://mozilla.org",
    tags: ["moz", "dot", "org"],
  }]), { needsMerge: false });
  await PlacesTestUtils.markBookmarksAsSynced();

  let changesToUpload = await buf.apply({
    weakUpload: ["mozBmk______"]
  });

  ok("mozBmk______" in changesToUpload);
  equal(changesToUpload.mozBmk______.counter, 0);

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
