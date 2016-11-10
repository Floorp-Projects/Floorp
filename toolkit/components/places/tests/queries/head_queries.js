/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cr = Components.results;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

// Import common head.
{
  let commonFile = do_get_file("../head_common.js", false);
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

// Put any other stuff relative to this test folder below.


// Some Useful Date constants - PRTime uses microseconds, so convert
const DAY_MICROSEC = 86400000000;
const today = PlacesUtils.toPRTime(Date.now());
const yesterday = today - DAY_MICROSEC;
const lastweek = today - (DAY_MICROSEC * 7);
const daybefore = today - (DAY_MICROSEC * 2);
const old = today - (DAY_MICROSEC * 3);
const futureday = today + (DAY_MICROSEC * 3);
const olderthansixmonths = today - (DAY_MICROSEC * 31 * 7);


/**
 * Generalized function to pull in an array of objects of data and push it into
 * the database.  It does NOT do any checking to see that the input is
 * appropriate.  This function is an asynchronous task, it can be called using
 * "Task.spawn" or using the "yield" function inside another task.
 */
function* task_populateDB(aArray)
{
  // Iterate over aArray and execute all instructions.
  for (let arrayItem of aArray) {
    try {
      // make the data object into a query data object in order to create proper
      // default values for anything left unspecified
      var qdata = new queryData(arrayItem);
      if (qdata.isVisit) {
        // Then we should add a visit for this node
        yield PlacesTestUtils.addVisits({
          uri: uri(qdata.uri),
          transition: qdata.transType,
          visitDate: qdata.lastVisit,
          referrer: qdata.referrer ? uri(qdata.referrer) : null,
          title: qdata.title
        });
        if (qdata.visitCount && !qdata.isDetails) {
          // Set a fake visit_count, this is not a real count but can be used
          // to test sorting by visit_count.
          let stmt = DBConn().createAsyncStatement(
            "UPDATE moz_places SET visit_count = :vc WHERE url_hash = hash(:url) AND url = :url");
          stmt.params.vc = qdata.visitCount;
          stmt.params.url = qdata.uri;
          try {
            stmt.executeAsync();
          }
          catch (ex) {
            print("Error while setting visit_count.");
          }
          finally {
            stmt.finalize();
          }
        }
      }

      if (qdata.isRedirect) {
        // This must be async to properly enqueue after the updateFrecency call
        // done by the visit addition.
        let stmt = DBConn().createAsyncStatement(
          "UPDATE moz_places SET hidden = 1 WHERE url_hash = hash(:url) AND url = :url");
        stmt.params.url = qdata.uri;
        try {
          stmt.executeAsync();
        }
        catch (ex) {
          print("Error while setting hidden.");
        }
        finally {
          stmt.finalize();
        }
      }

      if (qdata.isDetails) {
        // Then we add extraneous page details for testing
        yield PlacesTestUtils.addVisits({
          uri: uri(qdata.uri),
          visitDate: qdata.lastVisit,
          title: qdata.title
        });
      }

      if (qdata.markPageAsTyped) {
        PlacesUtils.history.markPageAsTyped(uri(qdata.uri));
      }

      if (qdata.isPageAnnotation) {
        if (qdata.removeAnnotation)
          PlacesUtils.annotations.removePageAnnotation(uri(qdata.uri),
                                                       qdata.annoName);
        else {
          PlacesUtils.annotations.setPageAnnotation(uri(qdata.uri),
                                                    qdata.annoName,
                                                    qdata.annoVal,
                                                    qdata.annoFlags,
                                                    qdata.annoExpiration);
        }
      }

      if (qdata.isItemAnnotation) {
        if (qdata.removeAnnotation)
          PlacesUtils.annotations.removeItemAnnotation(qdata.itemId,
                                                       qdata.annoName);
        else {
          PlacesUtils.annotations.setItemAnnotation(qdata.itemId,
                                                    qdata.annoName,
                                                    qdata.annoVal,
                                                    qdata.annoFlags,
                                                    qdata.annoExpiration);
        }
      }

      if (qdata.isFolder) {
        yield PlacesUtils.bookmarks.insert({
          parentGuid: qdata.parentGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          title: qdata.title,
          index: qdata.index
        });
      }

      if (qdata.isLivemark) {
        yield PlacesUtils.livemarks.addLivemark({ title: qdata.title
                                                , parentId: (yield PlacesUtils.promiseItemId(qdata.parentGuid))
                                                , index: qdata.index
                                                , feedURI: uri(qdata.feedURI)
                                                , siteURI: uri(qdata.uri)
                                                });
      }

      if (qdata.isBookmark) {
        let data = {
          parentGuid: qdata.parentGuid,
          index: qdata.index,
          title: qdata.title,
          url: qdata.uri
        };

        if (qdata.dateAdded) {
          data.dateAdded = new Date(qdata.dateAdded / 1000);
        }

        if (qdata.lastModified) {
          data.lastModified = new Date(qdata.lastModified / 1000);
        }

        yield PlacesUtils.bookmarks.insert(data);

        if (qdata.keyword) {
          yield PlacesUtils.keywords.insert({ url: qdata.uri,
                                              keyword: qdata.keyword });
        }
      }

      if (qdata.isTag) {
        PlacesUtils.tagging.tagURI(uri(qdata.uri), qdata.tagArray);
      }

      if (qdata.isSeparator) {
        yield PlacesUtils.bookmarks.insert({
          parentGuid: qdata.parentGuid,
          type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
          index: qdata.index
        });
      }
    } catch (ex) {
      // use the arrayItem object here in case instantiation of qdata failed
      do_print("Problem with this URI: " + arrayItem.uri);
      do_throw("Error creating database: " + ex + "\n");
    }
  }
}


/**
 * The Query Data Object - this object encapsulates data for our queries and is
 * used to parameterize our calls to the Places APIs to put data into the
 * database. It also has some interesting meta functions to determine which APIs
 * should be called, and to determine if this object should show up in the
 * resulting query.
 * Its parameter is an object specifying which attributes you want to set.
 * For ex:
 * var myobj = new queryData({isVisit: true, uri:"http://mozilla.com", title="foo"});
 * Note that it doesn't do any input checking on that object.
 */
function queryData(obj) {
  this.isVisit = obj.isVisit ? obj.isVisit : false;
  this.isBookmark = obj.isBookmark ? obj.isBookmark : false;
  this.uri = obj.uri ? obj.uri : "";
  this.lastVisit = obj.lastVisit ? obj.lastVisit : today;
  this.referrer = obj.referrer ? obj.referrer : null;
  this.transType = obj.transType ? obj.transType : Ci.nsINavHistoryService.TRANSITION_TYPED;
  this.isRedirect = obj.isRedirect ? obj.isRedirect : false;
  this.isDetails = obj.isDetails ? obj.isDetails : false;
  this.title = obj.title ? obj.title : "";
  this.markPageAsTyped = obj.markPageAsTyped ? obj.markPageAsTyped : false;
  this.isPageAnnotation = obj.isPageAnnotation ? obj.isPageAnnotation : false;
  this.removeAnnotation = obj.removeAnnotation ? true : false;
  this.annoName = obj.annoName ? obj.annoName : "";
  this.annoVal = obj.annoVal ? obj.annoVal : "";
  this.annoFlags = obj.annoFlags ? obj.annoFlags : 0;
  this.annoExpiration = obj.annoExpiration ? obj.annoExpiration : 0;
  this.isItemAnnotation = obj.isItemAnnotation ? obj.isItemAnnotation : false;
  this.itemId = obj.itemId ? obj.itemId : 0;
  this.annoMimeType = obj.annoMimeType ? obj.annoMimeType : "";
  this.isTag = obj.isTag ? obj.isTag : false;
  this.tagArray = obj.tagArray ? obj.tagArray : null;
  this.isLivemark = obj.isLivemark ? obj.isLivemark : false;
  this.parentGuid = obj.parentGuid || PlacesUtils.bookmarks.rootGuid;
  this.feedURI = obj.feedURI ? obj.feedURI : "";
  this.index = obj.index ? obj.index : PlacesUtils.bookmarks.DEFAULT_INDEX;
  this.isFolder = obj.isFolder ? obj.isFolder : false;
  this.contractId = obj.contractId ? obj.contractId : "";
  this.lastModified = obj.lastModified ? obj.lastModified : null;
  this.dateAdded = obj.dateAdded ? obj.dateAdded : null;
  this.keyword = obj.keyword ? obj.keyword : "";
  this.visitCount = obj.visitCount ? obj.visitCount : 0;
  this.isSeparator = obj.hasOwnProperty("isSeparator") && obj.isSeparator;

  // And now, the attribute for whether or not this object should appear in the
  // resulting query
  this.isInQuery = obj.isInQuery ? obj.isInQuery : false;
}

// All attributes are set in the constructor above
queryData.prototype = { }


/**
 * Helper function to compare an array of query objects with a result set.
 * It assumes the array of query objects contains the SAME SORT as the result
 * set.  It checks the the uri, title, time, and bookmarkIndex properties of
 * the results, where appropriate.
 */
function compareArrayToResult(aArray, aRoot) {
  do_print("Comparing Array to Results");

  var wasOpen = aRoot.containerOpen;
  if (!wasOpen)
    aRoot.containerOpen = true;

  // check expected number of results against actual
  var expectedResultCount = aArray.filter(function(aEl) { return aEl.isInQuery; }).length;
  if (expectedResultCount != aRoot.childCount) {
    // Debugging code for failures.
    dump_table("moz_places");
    dump_table("moz_historyvisits");
    do_print("Found children:");
    for (let i = 0; i < aRoot.childCount; i++) {
      do_print(aRoot.getChild(i).uri);
    }
    do_print("Expected:");
    for (let i = 0; i < aArray.length; i++) {
      if (aArray[i].isInQuery)
        do_print(aArray[i].uri);
    }
  }
  do_check_eq(expectedResultCount, aRoot.childCount);

  var inQueryIndex = 0;
  for (var i = 0; i < aArray.length; i++) {
    if (aArray[i].isInQuery) {
      var child = aRoot.getChild(inQueryIndex);
      // do_print("testing testData[" + i + "] vs result[" + inQueryIndex + "]");
      if (!aArray[i].isFolder && !aArray[i].isSeparator) {
        do_print("testing testData[" + aArray[i].uri + "] vs result[" + child.uri + "]");
        if (aArray[i].uri != child.uri) {
          dump_table("moz_places");
          do_throw("Expected " + aArray[i].uri + " found " + child.uri);
        }
      }
      if (!aArray[i].isSeparator && aArray[i].title != child.title)
        do_throw("Expected " + aArray[i].title + " found " + child.title);
      if (aArray[i].hasOwnProperty("lastVisit") &&
          aArray[i].lastVisit != child.time)
        do_throw("Expected " + aArray[i].lastVisit + " found " + child.time);
      if (aArray[i].hasOwnProperty("index") &&
          aArray[i].index != PlacesUtils.bookmarks.DEFAULT_INDEX &&
          aArray[i].index != child.bookmarkIndex)
        do_throw("Expected " + aArray[i].index + " found " + child.bookmarkIndex);

      inQueryIndex++;
    }
  }

  if (!wasOpen)
    aRoot.containerOpen = false;
  do_print("Comparing Array to Results passes");
}


/**
 * Helper function to check to see if one object either is or is not in the
 * result set.  It can accept either a queryData object or an array of queryData
 * objects.  If it gets an array, it only compares the first object in the array
 * to see if it is in the result set.
 * Returns: True if item is in query set, and false if item is not in query set
 *          If input is an array, returns True if FIRST object in array is in
 *          query set.  To compare entire array, use the function above.
 */
function isInResult(aQueryData, aRoot) {
  var rv = false;
  var uri;
  var wasOpen = aRoot.containerOpen;
  if (!wasOpen)
    aRoot.containerOpen = true;

  // If we have an array, pluck out the first item. If an object, pluc out the
  // URI, we just compare URI's here.
  if ("uri" in aQueryData) {
    uri = aQueryData.uri;
  } else {
    uri = aQueryData[0].uri;
  }

  for (var i = 0; i < aRoot.childCount; i++) {
    if (uri == aRoot.getChild(i).uri) {
      rv = true;
      break;
    }
  }
  if (!wasOpen)
    aRoot.containerOpen = false;
  return rv;
}


/**
 * A nice helper function for debugging things. It prints the contents of a
 * result set.
 */
function displayResultSet(aRoot) {

  var wasOpen = aRoot.containerOpen;
  if (!wasOpen)
    aRoot.containerOpen = true;

  if (!aRoot.hasChildren) {
    // Something wrong? Empty result set?
    do_print("Result Set Empty");
    return;
  }

  for (var i = 0; i < aRoot.childCount; ++i) {
    do_print("Result Set URI: " + aRoot.getChild(i).uri + "   Title: " +
        aRoot.getChild(i).title + "   Visit Time: " + aRoot.getChild(i).time);
  }
  if (!wasOpen)
    aRoot.containerOpen = false;
}
