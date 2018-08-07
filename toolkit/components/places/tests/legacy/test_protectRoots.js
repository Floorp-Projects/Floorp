/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async () => {
  const ROOTS = [
    PlacesUtils.bookmarks.rootGuid,
    ...PlacesUtils.bookmarks.userContentRoots,
    PlacesUtils.bookmarks.tagsGuid,
  ];

  for (let guid of ROOTS) {
    Assert.ok(PlacesUtils.isRootItem(guid));

    let id = await PlacesUtils.promiseItemId(guid);

    try {
      PlacesUtils.bookmarks.removeItem(id);
      do_throw("Trying to remove a root should throw");
    } catch (ex) {}
  }
});
