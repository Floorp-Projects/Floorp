/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Drew Willcoxon <adw@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Bug 455315
 * https://bugzilla.mozilla.org/show_bug.cgi?id=412132
 *
 * Ensures that the frecency of a bookmark's URI is what it should be after the
 * bookmark is deleted.
 */

const bmServ = PlacesUtils.bookmarks;
const histServ = PlacesUtils.history;
const lmServ = PlacesUtils.livemarks;

let tests = [

{
  desc: ["Frecency of unvisited, separately bookmarked livemark item's URI ",
         "should be zero after bookmark removed."].join(""),
  run: function ()
  {
    // Add livemark and bookmark.  Bookmark's URI is the URI of the livemark's
    // only item.
    let lmItemURL = "http://example.com/livemark-item";
    let lmItemURI = uri(lmItemURL);
    createLivemark(lmItemURI);
    let bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                     lmItemURI,
                                     bmServ.DEFAULT_INDEX,
                                     "bookmark title");
    waitForFrecency(lmItemURL, function(aFrecency) aFrecency > 0,
                    this.check1, this, [lmItemURL, bmId]);
  },
  check1: function (aUrl, aItemId)
  {
    print("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);

    bmServ.removeItem(aItemId);
    waitForFrecency(aUrl, function(aFrecency) aFrecency == 0,
                    this.check2, this, [aUrl]);
  },
  check2: function (aUrl)
  {
    print("URI's only bookmark is now unvisited livemark item => frecency = 0");
    do_check_eq(frecencyForUrl(aUrl), 0);
    run_next_test();
  }
},

{
  desc: ["Frecency of visited, separately bookmarked livemark item's URI ",
         "should not be zero after bookmark removed."].join(""),
  run: function ()
  {
    // Add livemark and bookmark.  Bookmark's URI is the URI of the livemark's
    // only item.
    let lmItemURL = "http://example.com/livemark-item";
    let lmItemURI = uri(lmItemURL);
    createLivemark(lmItemURI);
    let bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                     lmItemURI,
                                     bmServ.DEFAULT_INDEX,
                                     "bookmark title");
    waitForFrecency(lmItemURL, function(aFrecency) aFrecency > 0,
                    this.check1, this, [lmItemURL, bmId]);
  },
  check1: function (aUrl, aItemId)
  {
    print("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);

    visit(uri(aUrl));
    bmServ.removeItem(aItemId);
    waitForFrecency(aUrl, function(aFrecency) aFrecency > 0,
                    this.check2, this, [aUrl]);
  },
  check2: function (aUrl)
  {
    print("URI's only bookmark is now *visited* livemark item => frecency != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);
    run_next_test();
  }
},

{
  desc: ["After removing bookmark, frecency of bookmark's URI should be zero ",
         "if URI is unvisited and no longer bookmarked."].join(""),
  run: function ()
  {
    let url = "http://example.com/1";
    let bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                     uri(url),
                                     bmServ.DEFAULT_INDEX,
                                     "bookmark title");
    waitForFrecency(url, function(aFrecency) aFrecency > 0,
                    this.check1, this, [url, bmId]);
  },
  check1: function (aUrl, aItemId)
  {
    print("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);

    bmServ.removeItem(aItemId);
    waitForFrecency(aUrl, function(aFrecency) aFrecency == 0,
                    this.check2, this, [aUrl]);
  },
  check2: function (aUrl)
  {
    print("Unvisited URI no longer bookmarked => frecency should = 0");
    do_check_eq(frecencyForUrl(aUrl), 0);
    run_next_test();
  }
},

{
  desc: ["After removing bookmark, frecency of bookmark's URI should not be ",
         "zero if URI is visited."].join(""),
  run: function ()
  {
    let bmURL = "http://example.com/1";
    let bmURI = uri(bmURL);

    let bmId = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                     bmURI,
                                     bmServ.DEFAULT_INDEX,
                                     "bookmark title");
    waitForFrecency(bmURL, function(aFrecency) aFrecency > 0,
                    this.check1, this, [bmURL, bmId]);
  },
  check1: function (aUrl, aItemId)
  {
    print("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);

    visit(uri(aUrl));
    bmServ.removeItem(aItemId);
    waitForFrecency(aUrl, function(aFrecency) aFrecency > 0,
                    this.check2, this, [aUrl]);
  },
  check2: function (aUrl)
  {
    print("*Visited* URI no longer bookmarked => frecency should != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);
    run_next_test();
  }
},

{
  desc: ["After removing bookmark, frecency of bookmark's URI should not be ",
         "zero if URI is still bookmarked."].join(""),
  run: function ()
  {
    let bmURL = "http://example.com/1";
    let bmURI = uri(bmURL);

    let bm1Id = bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                                      bmURI,
                                      bmServ.DEFAULT_INDEX,
                                      "bookmark 1 title");

    bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                          bmURI,
                          bmServ.DEFAULT_INDEX,
                          "bookmark 2 title");
    waitForFrecency(bmURL, function(aFrecency) aFrecency > 0,
                    this.check1, this, [bmURL, bm1Id]);
  },
  check1: function (aUrl, aItemId)
  {
    print("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);

    bmServ.removeItem(aItemId);
    waitForFrecency(aUrl, function(aFrecency) aFrecency > 0,
                    this.check2, this, [aUrl]);
  },
  check2: function (aUrl)
  {
    print("URI still bookmarked => frecency should != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);
    run_next_test();
  }
},

{
  desc: ["Frecency of unvisited, separately bookmarked livemark item's URI ",
         "should be zero after all children removed from bookmark's ",
         "parent."].join(""),
  run: function ()
  {
    // Add livemark and bookmark.  Bookmark's URI is the URI of the livemark's
    // only item.
    let lmItemURL = "http://example.com/livemark-item";
    let lmItemURI = uri(lmItemURL);
    createLivemark(lmItemURI);

    bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                          lmItemURI,
                          bmServ.DEFAULT_INDEX,
                          "bookmark title");
    waitForFrecency(lmItemURL, function(aFrecency) aFrecency > 0,
                    this.check1, this, [lmItemURL]);
  },
  check1: function (aUrl)
  {
    print("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);

    bmServ.removeFolderChildren(bmServ.unfiledBookmarksFolder);
    waitForFrecency(aUrl, function(aFrecency) aFrecency == 0,
                    this.check2, this, [aUrl]);
  },
  check2: function (aUrl)
  {
    print("URI's only bookmark is now unvisited livemark item => frecency = 0");
    do_check_eq(frecencyForUrl(aUrl), 0);
    run_next_test();
  }
},

{
  desc: ["Frecency of visited, separately bookmarked livemark item's URI ",
         "should not be zero after all children removed from bookmark's ",
         "parent."].join(""),
  run: function ()
  {
    // Add livemark and bookmark.  Bookmark's URI is the URI of the livemark's
    // only item.
    let lmItemURL = "http://example.com/livemark-item";
    let lmItemURI = uri(lmItemURL);
    createLivemark(lmItemURI);
    bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                          lmItemURI,
                          bmServ.DEFAULT_INDEX,
                          "bookmark title");
    waitForFrecency(lmItemURL, function(aFrecency) aFrecency > 0,
                    this.check1, this, [lmItemURL]);
  },
  check1: function (aUrl, aItemId)
  {
    print("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);

    visit(uri(aUrl));
    bmServ.removeFolderChildren(bmServ.unfiledBookmarksFolder);
    waitForFrecency(aUrl, function(aFrecency) aFrecency > 0,
                    this.check2, this, [aUrl]);
  },
  check2: function (aUrl)
  {
    print("URI's only bookmark is now *visited* livemark item => frecency != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);
    run_next_test();
  }
},

{
  desc: ["After removing all children from bookmark's parent, frecency of ",
         "bookmark's URI should be zero if URI is unvisited and no longer ",
         "bookmarked."].join(""),
  run: function ()
  {
    let url = "http://example.com/1";
    bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                          uri(url),
                          bmServ.DEFAULT_INDEX,
                          "bookmark title");
    waitForFrecency(url, function(aFrecency) aFrecency > 0,
                    this.check1, this, [url]);
  },
  check1: function (aUrl, aItemId)
  {
    print("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);

    bmServ.removeFolderChildren(bmServ.unfiledBookmarksFolder);
    waitForFrecency(aUrl, function(aFrecency) aFrecency == 0,
                    this.check2, this, [aUrl]);
  },
  check2: function (aUrl)
  {
    print("Unvisited URI no longer bookmarked => frecency should = 0");
    do_check_eq(frecencyForUrl(aUrl), 0);
    run_next_test();
  }
},

{
  desc: ["After removing all children from bookmark's parent, frecency of ",
         "bookmark's URI should not be zero if URI is visited."].join(""),
  run: function ()
  {
    let bmURL = "http://example.com/1";
    let bmURI = uri(bmURL);

    bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                          bmURI,
                          bmServ.DEFAULT_INDEX,
                          "bookmark title");
    waitForFrecency(bmURL, function(aFrecency) aFrecency > 0,
                    this.check1, this, [bmURL]);
  },
  check1: function (aUrl)
  {
    print("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);

    visit(uri(aUrl));
    bmServ.removeFolderChildren(bmServ.unfiledBookmarksFolder);
    waitForFrecency(aUrl, function(aFrecency) aFrecency > 0,
                    this.check2, this, [aUrl]);
  },
  check2: function (aUrl)
  {
    print("*Visited* URI no longer bookmarked => frecency should != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);
    run_next_test();
  }
},

{
  desc: ["After removing all children from bookmark's parent, frecency of ",
         "bookmark's URI should not be zero if URI is still ",
         "bookmarked."].join(""),
  run: function ()
  {
    let bmURL = "http://example.com/1";
    let bmURI = uri(bmURL);

    bmServ.insertBookmark(bmServ.toolbarFolder,
                          bmURI,
                          bmServ.DEFAULT_INDEX,
                          "bookmark 1 title");

    bmServ.insertBookmark(bmServ.unfiledBookmarksFolder,
                          bmURI,
                          bmServ.DEFAULT_INDEX,
                          "bookmark 2 title");
    waitForFrecency(bmURL, function(aFrecency) aFrecency > 0,
                    this.check1, this, [bmURL]);
  },
  check1: function (aUrl)
  {
    print("Bookmarked => frecency of URI should be != 0");
    do_check_neq(frecencyForUrl(aUrl), 0);

    bmServ.removeFolderChildren(bmServ.unfiledBookmarksFolder);
    waitForFrecency(aUrl, function(aFrecency) aFrecency > 0,
                    this.check2, this, [aUrl]);
  },
  check2: function (aUrl)
  {
    // URI still bookmarked => frecency should != 0.
    do_check_neq(frecencyForUrl(aUrl), 0);
    run_next_test();
  }
},

];

///////////////////////////////////////////////////////////////////////////////

/**
 * Creates a livemark with a single child item.
 *
 * @param  aLmChildItemURI
 *         the URI of the livemark's single child item
 * @return the item ID of the single child item
 */
function createLivemark(aLmChildItemURI) {
  let lmItemId = lmServ.createLivemarkFolderOnly(bmServ.unfiledBookmarksFolder,
                                                 "livemark title",
                                                 uri("http://example.com/"),
                                                 uri("http://example.com/rdf"),
                                                 -1);
  return bmServ.insertBookmark(lmItemId,
                               aLmChildItemURI,
                               bmServ.DEFAULT_INDEX,
                               "livemark item title");
}

/**
 * Adds a visit for aURI.
 *
 * @param aURI
 *        the URI of the Place for which to add a visit
 */
function visit(aURI) {
  let visitId = histServ.addVisit(aURI,
                                  Date.now() * 1000,
                                  null,
                                  histServ.TRANSITION_BOOKMARK,
                                  false,
                                  0);
}

///////////////////////////////////////////////////////////////////////////////

function run_test() {
  do_test_pending();
  run_next_test();
}

function run_next_test() {
  if (tests.length) {
    let test = tests.shift();
    print("\n ***Test: " + test.desc);
    remove_all_bookmarks();
    waitForClearHistory(function() {
      test.run.call(test);
    });
  }
  else {
    do_test_finished();
  }
}
