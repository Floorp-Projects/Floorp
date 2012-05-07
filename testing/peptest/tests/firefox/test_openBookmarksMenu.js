/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Import mozmill and initialize a controller
Components.utils.import('resource://mozmill/driver/mozmill.js');
let c = getBrowserController();

c.open('http://mozilla.org');
c.waitForPageLoad();

let bookmark = findElement.ID(c.window.document, "bookmarksMenu");
pep.performAction('scroll_bookmarks', function() {
  bookmark.click();
  for (let i = 0; i < 15; ++i) {
    bookmark.keypress('VK_DOWN');
    // Sleep to better emulate a user
    c.sleep(10);
  }
});

let showall = findElement.ID(c.window.document, "bookmarksShowAll");
pep.performAction('show_all_bookmarks', function() {
  showall.click();
});

pep.getWindow("Places:Organizer").close();