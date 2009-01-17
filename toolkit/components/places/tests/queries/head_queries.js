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
 * The Original Code is Places Test Code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Clint Talbert <ctalbert@mozilla.com>
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
const NS_APP_USER_PROFILE_50_DIR = "ProfD";
const NS_APP_HISTORY_50_FILE = "UHist";

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

// Some Useful Date constants - PRTime uses microseconds, so convert
const DAY_MICROSEC = 86400000000;
const today = Date.now() * 1000;
const yesterday = today - DAY_MICROSEC;
const lastweek = today - (DAY_MICROSEC * 7);
const daybefore = today - (DAY_MICROSEC * 2);
const tomorrow = today + DAY_MICROSEC;
const old = today - (DAY_MICROSEC * 3);
const futureday = today + (DAY_MICROSEC * 3);

function LOG(aMsg) {
 aMsg = ("*** PLACES TESTS: " + aMsg);
 print(aMsg);
}

// If there's no location registered for the profile directory, register one now.
var dirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
var profileDir = null;
try {
 profileDir = dirSvc.get(NS_APP_USER_PROFILE_50_DIR, Ci.nsIFile);
} catch (e) {}
if (!profileDir) {
 // Register our own provider for the profile directory.
 // It will simply return the current directory.
 var provider = {
   getFile: function(prop, persistent) {
     persistent.value = true;
     if (prop == NS_APP_USER_PROFILE_50_DIR) {
       return dirSvc.get("CurProcD", Ci.nsIFile);
     }
     if (prop == NS_APP_HISTORY_50_FILE) {
       var histFile = dirSvc.get("CurProcD", Ci.nsIFile);
       histFile.append("history.dat");
       return histFile;
     }
     throw Cr.NS_ERROR_FAILURE;
   },
   QueryInterface: function(iid) {
     if (iid.equals(Ci.nsIDirectoryServiceProvider) ||
         iid.equals(Ci.nsISupports)) {
       return this;
     }
     throw Cr.NS_ERROR_NO_INTERFACE;
   }
 };
 dirSvc.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);
}

var iosvc = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);

function uri(spec) {
 return iosvc.newURI(spec, null, null);
}

// Delete a previously created sqlite file
function clearDB() {
 try {
   var file = dirSvc.get('ProfD', Ci.nsIFile);
   file.append("places.sqlite");
   if (file.exists())
     file.remove(false);
 } catch(ex) { dump("Exception: " + ex); }
}
clearDB();

// Get our interfaces 
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
}

try {
  var bhistsvc = histsvc.QueryInterface(Ci.nsIBrowserHistory);
} catch(ex) {
  do_throw("Could not get browser history service\n");
}

try {
  var annosvc = Cc["@mozilla.org/browser/annotation-service;1"].
                getService(Ci.nsIAnnotationService);
} catch(ex) {
  do_throw("Could not get annotation service\n");
}

try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
} catch (ex) {
  do_throw("Could not get bookmark service\n");
}

try {
  var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].
                getService(Ci.nsITaggingService);
} catch(ex) {
  do_throw("Could not get tagging service\n");
}

try {
  var faviconsvc = Cc["@mozilla.org/browser/favicon-service;1"].
                   getService(Ci.nsIFaviconService);
} catch(ex) {
  do_throw("Could not get favicon service\n");
}

try {
  var lmsvc = Cc["@mozilla.org/browser/livemark-service;2"].
              getService(Ci.nsILivemarkService);
} catch(ex) {
  do_throw("Could not get livemark service\n");
}

/**
 * Generalized function to pull in an array of objects of data and push it into
 * the database. It does NOT do any checking to see that the input is
 * appropriate.
 */
function populateDB(aArray) {
  aArray.forEach(function(data) {
    try {
      // make the data object into a query data object in order to create proper
      // default values for anything left unspecified
      var qdata = new queryData(data);
      if (qdata.isVisit) {
        // Then we should add a visit for this node
        var referrer = qdata.referrer ? uri(qdata.referrer) : null;
        var visitId = histsvc.addVisit(uri(qdata.uri), qdata.lastVisit,
                                       referrer, qdata.transType,
                                       qdata.isRedirect, qdata.sessionID);
        do_check_true(visitId > 0);
      }

      if (qdata.isDetails) {
        // Then we add extraneous page details for testing
        bhistsvc.addPageWithDetails(uri(qdata.uri), qdata.title, qdata.lastVisit);
      }

      if (qdata.markPageAsTyped){
        bhistsvc.markPageAsTyped(uri(qdata.uri));
      }

      if (qdata.hidePage){
        bhistsvc.hidePage(uri(qdata.uri));
      }

      if (qdata.isPageAnnotation) {
        annosvc.setPageAnnotation(uri(qdata.uri), qdata.annoName, qdata.annoVal,
                                  qdata.annoFlags, qdata.annoExpiration);
      }

      if (qdata.isItemAnnotation) {
        annosvc.setItemAnnotation(qdata.itemId, qdata.annoName, qdata.annoVal,
                                  qdata.annoFlags, qdata.annoExpiration);
      }

      if (qdata.isPageBinaryAnnotation) {
        annosvc.setPageAnnotationBinary(uri(qdata.uri), qdata.annoName,
                                        qdata.binarydata, qdata.binaryDataLength,
                                        qdata.annoMimeType, qdata.annoFlags,
                                        qdata.annoExpiration);
      }

      if (qdata.isItemBinaryAnnotation) {
        annosvc.setItemAnnotationBinary(qdata.itemId, qdata.annoName,
                                        qdata.binaryData, qdata.binaryDataLength,
                                        qdata.annoMimeType, qdata.annoFlags,
                                        qdata.annoExpiration);
      }

      if (qdata.isTag) {
        tagssvc.tagURI(uri(qdata.uri), qdata.tagArray);
      }

      if (qdata.isFavicon) {
        // Not planning on doing deep testing of favIcon service so these two
        // calls should be sufficient to get favicons into the database
        try {
          faviconsvc.setFaviconData(uri(qdata.faviconURI), qdata.favicon,
                                    qdata.faviconLen, qdata.faviconMimeType,
                                    qdata.faviconExpiration);
        } catch (ex) {}
        faviconsvc.setFaviconUrlForPage(uri(qdata.uri), uri(qdata.faviconURI));
      }

      if (qdata.isFolder) {
        bmsvc.createFolder(qdata.parentFolder, qdata.title, qdata.index);
      }

      if (qdata.isLivemark) {
        lmsvc.createLivemark(qdata.parentFolder, qdata.title, uri(qdata.uri),
                       uri(qdata.feedURI), qdata.index);
      }

      if (qdata.isBookmark) {
        let itemId = bmsvc.insertBookmark(qdata.parentFolder, uri(qdata.uri),
                                          qdata.index, qdata.title);
        if (qdata.keyword)
          bmsvc.setKeywordForBookmark(itemId, qdata.keyword);
        if (qdata.dateAdded)
          bmsvc.setItemDateAdded(itemId, qdata.dateAdded);
        if (qdata.lastModified)
          bmsvc.setItemLastModified(itemId, qdata.lastModified);

        LOG("added bookmark");
      }

      if (qdata.isDynContainer) {
        bmsvc.createDynamicContainer(qdata.parentFolder, qdata.title,
                                       qdata.contractId, qdata.index);
      }
    } catch (ex) {
      // use the data object here in case instantiation of qdata failed
      LOG("Problem with this URI: " + data.uri);
      do_throw("Error creating database: " + ex + "\n");
    }
  }); // End of function and array
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
  this.isBookmark = obj.isBookmark ? obj.isBookmark: false;
  this.uri = obj.uri ? obj.uri : "";
  this.lastVisit = obj.lastVisit ? obj.lastVisit : today;
  this.referrer = obj.referrer ? obj.referrer : null;
  this.transType = obj.transType ? obj.transType : histsvc.TRANSITION_TYPED;
  this.isRedirect = obj.isRedirect ? obj.isRedirect : false;
  this.sessionID = obj.sessionID ? obj.sessionID : 0;
  this.isDetails = obj.isDetails ? obj.isDetails : false;
  this.title = obj.title ? obj.title : "";
  this.markPageAsTyped = obj.markPageAsTyped ? obj.markPageAsTyped : false;
  this.hidePage = obj.hidePage ? obj.hidePage : false;
  this.isPageAnnotation = obj.isPageAnnotation ? obj.isPageAnnotation : false;
  this.annoName = obj.annoName ? obj.annoName : "";
  this.annoVal = obj.annoVal ? obj.annoVal : "";
  this.annoFlags = obj.annoFlags ? obj.annoFlags : 0;
  this.annoExpiration = obj.annoExpiration ? obj.annoExpiration : 0;
  this.isItemAnnotation = obj.isItemAnnotation ? obj.isItemAnnotation : false;
  this.itemId = obj.itemId ? obj.itemId : 0;
  this.isPageBinaryAnnotation = obj.isPageBinaryAnnotation ?
                                obj.isPageBinaryAnnotation : false;
  this.isItemBinaryAnnotation = obj.isItemBinaryAnnotation ?
                                obj.isItemBinaryAnnotation : false;
  this.binaryData = obj.binaryData ? obj.binaryData : null;
  this.binaryDataLength = obj.binaryDataLength ? obj.binaryDataLength : 0;
  this.annoMimeType = obj.annoMimeType ? obj.annoMimeType : "";
  this.isTag = obj.isTag ? obj.isTag : false;
  this.tagArray = obj.tagArray ? obj.tagArray : null;
  this.isFavicon = obj.isFavicon ? obj.isFavicon : false;
  this.faviconURI = obj.faviconURI ? obj.faviconURI : "";
  this.faviconLen = obj.faviconLen ? obj.faviconLen : 0;
  this.faviconMimeType = obj.faviconMimeType ? obj.faviconMimeType : "";
  this.faviconExpiration = obj.faviconExpiration ?
                           obj.faviconExpiration : futureday;
  this.isLivemark = obj.isLivemark ? obj.isLivemark : false;
  this.parentFolder = obj.parentFolder ? obj.parentFolder : bmsvc.placesRoot;
  this.feedURI = obj.feedURI ? obj.feedURI : "";
  this.bmIndex = obj.bmIndex ? obj.bmIndex : bmsvc.DEFAULT_INDEX;
  this.isFolder = obj.isFolder ? obj.isFolder : false;
  this.contractId = obj.contractId ? obj.contractId : "";

  // And now, the attribute for whether or not this object should appear in the
  // resulting query
  this.isInQuery = obj.isInQuery ? obj.isInQuery : false;
}

// All attributes are set in the constructor above
queryData.prototype = { }

/**
 * Helper function to compare an array of query objects with a result set
 * NOTE: It assumes the array of query objects contains the SAME SORT as the
 *       result set and only checks the URI and title of the results.
 *       For deeper checks, you'll need to write your own method.
 */
function compareArrayToResult(aArray, aRoot) {
  LOG("Comparing Array to Results");

  if (!aRoot.containerOpen)
    aRoot.containerOpen = true;

  // check expected number of results against actual
  var expectedResultCount = aArray.filter(function(aEl) { return aEl.isInQuery; }).length;
  do_check_eq(expectedResultCount, aRoot.childCount);

  var inQueryIndex = 0;
  for (var i=0; i < aArray.length; i++) {
    if (aArray[i].isInQuery) {
      var child = aRoot.getChild(inQueryIndex);
      LOG("testing testData[" + i + "] vs result[" + inQueryIndex + "]");
      LOG("testing testData[" + aArray[i].uri + "] vs result[" + child.uri + "]");
      //do_check_eq(aArray[i].uri, child.uri);
      //do_check_eq(aArray[i].title, child.title);
      inQueryIndex++;
    }
  }
  LOG("Comparing Array to Results passes");
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
  if (!aRoot.containerOpen)
    aRoot.containerOpen = true;

  // If we have an array, pluck out the first item. If an object, pluc out the
  // URI, we just compare URI's here.
  if ("uri" in aQueryData) {
    uri = aQueryData.uri;
  } else {
    uri = aQueryData[0].uri;
  }

  for (var i=0; i < aRoot.childCount; i++) {
    if (uri == aRoot.getChild(i).uri) {
      rv = true;
      break;
    }
  }
  return rv;
}

/**
 * A nice helper function for debugging things. It LOGs the contents of a
 * result set.
 */
function displayResultSet(aRoot) {

  if (!aRoot.containerOpen)
    aRoot.containerOpen = true;

  if (!aRoot.hasChildren) {
    // Something wrong? Empty result set?
    LOG("Result Set Empty");
    return;
  }

  for (var i=0; i < aRoot.childCount; ++i) {
    LOG("Result Set URI: " + aRoot.getChild(i).uri + " Title: " +
        aRoot.getChild(i).title);
  }
}

/*
 * Removes all bookmarks and checks for correct cleanup
 */
function remove_all_bookmarks() {
  // Clear all bookmarks
  bmsvc.removeFolderChildren(bmsvc.bookmarksMenuFolder);
  bmsvc.removeFolderChildren(bmsvc.toolbarFolder);
  bmsvc.removeFolderChildren(bmsvc.unfiledBookmarksFolder);
  // Check for correct cleanup
  check_no_bookmarks()
}

/*
 * Checks that we don't have any bookmark
 */
function check_no_bookmarks() {
  var query = histsvc.getNewQuery();
  query.setFolders([bmsvc.toolbarFolder, bmsvc.bookmarksMenuFolder, bmsvc.unfiledBookmarksFolder], 3);
  var options = histsvc.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 0);
  root.containerOpen = false;
}
