/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks that changing a tag for a bookmark with multiple tags
// notifies bookmark-tags-changed event only once, and not once per tag.

add_task(async function run_test() {
  let tags = ["a", "b", "c"];
  let uri = Services.io.newURI("http://1.moz.org/");

  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: uri,
    title: "Bookmark 1",
  });
  PlacesUtils.tagging.tagURI(uri, tags);

  let promise = Promise.withResolvers();

  let bookmarksObserver = {
    _changedCount: 0,
    handlePlacesEvents(events) {
      for (let event of events) {
        switch (event.type) {
          case "bookmark-removed":
            if (event.guid == bookmark.guid) {
              PlacesUtils.observers.removeListener(
                ["bookmark-removed"],
                this.handlePlacesEvents
              );
              Assert.equal(this._changedCount, 2);
              promise.resolve();
            }
            break;
          case "bookmark-tags-changed":
            Assert.equal(event.guid, bookmark.guid);
            this._changedCount++;
            break;
        }
      }
    },
  };
  bookmarksObserver.handlePlacesEvents =
    bookmarksObserver.handlePlacesEvents.bind(bookmarksObserver);
  PlacesUtils.observers.addListener(
    ["bookmark-removed", "bookmark-tags-changed"],
    bookmarksObserver.handlePlacesEvents
  );

  PlacesUtils.tagging.tagURI(uri, ["d"]);
  PlacesUtils.tagging.tagURI(uri, ["e"]);

  await promise;

  await PlacesUtils.bookmarks.remove(bookmark.guid);
});
