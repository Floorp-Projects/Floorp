Cu.import("resource://weave/engines/bookmarks.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/sharing.js");

Function.prototype.async = Async.sugar;

load("bookmark_setup.js");

function FakeMicrosummaryService() {
  return {hasMicrosummary: function() { return false; }};
}

function FakeAnnotationService() {
  this._annotations = {};
}
FakeAnnotationService.prototype = {
  EXPIRE_NEVER: 0,
  getItemAnnotation: function (aItemId, aName) {
    if (this._annotations[aItemId] != undefined)
      if (this._annotations[aItemId][aName])
	return this._annotations[aItemId][aName];
    return null;
  },
  setItemAnnotation: function (aItemId, aName, aValue, aFlags, aExpiration) {
    if (this._annotations[aItemId] == undefined)
      this._annotations[aItemId] = {};
    this._annotations[aItemId][aName] = aValue;
    dump( "Annotated item " + aItemId + " with " + aName + " = " + aValue + "\n");
    //ignore flags and expiration
  },
  getItemsWithAnnotation: function(aName, resultCount, results) {
    var list = [];
    for ( var x in this._annotations) {
      if (this._annotations[x][aName] != undefined) {
        return x;
      }
    }
    return list;
  }
}


function FakeSharingApi() {
}
FakeSharingApi.prototype = {
  shareWithUsers: function FakeSharingApi_shareWith(path, users, onComplete) {
    // TODO just set a flag on the fake DAV thing.
  }
}
Sharing.Api = FakeSharingApi;


var annoSvc = new FakeAnnotationService();

function makeBookmarksEngine() {
  let engine = new BookmarksEngine();
  engine._store.__ms = new FakeMicrosummaryService();
  let shareManager = engine._sharing;
  shareManager.__annoSvc = annoSvc; // use fake annotation service
  return engine;
}

function run_test() {
  let bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
    getService(Ci.nsINavBookmarksService);


  var syncTesting = new SyncTestingInfrastructure( makeBookmarksEngine );

  let folderName = "Funny Pictures of Manatees and Walruses";
  let folderToShare = bms.createFolder( bms.bookmarksMenuFolder,
					folderName, -1 );
  let lolrusBm = bms.insertBookmark(folderToShare,
				    uri("http://www.lolrus.com"),
				    -1, "LOLrus" );
  let lolateeBm = bms.insertBookmark(folderToShare,
				    uri("http://www.lolatee.com"),
				    -1, "LOLatee" );

  // Note xmpp.enabled is set to false by the SyncTestingInfrastructure.

  let username = "rusty";
  let engine = makeBookmarksEngine();
  let shareManager = engine._sharing;

  function setupShare(cb) {
    // TODO: Passing in folderToShare won't work at the time of writing
    // this because folderToShare is expected to be a DOM node, not a
    // Places ID.
    shareManager._share.async( shareManager, cb, folderToShare, "jonas" );
  }

  /*
  syncTesting.runAsyncFunc("Share folder with Jonas", setupShare);


  dump( "folderToShare = " + folderToShare + "\n");
  // Get the server path from folder annotation...
  let serverPath = annoSvc.getItemAnnotation( folderToShare,
					      "weave/shared-server-path" );
  dump( "Shared it to server path " + serverPath + "\n");

  // get off rusty's computer, switch to Jonas's computer:
  syncTesting.saveClientState( "rusty computer 1" );
  syncTesting.resetClientState();

  // These next two lines simulate what would happen when jonas received
  // the xmpp message and clicked "accept".
  shareManager._createIncomingShare(username, serverPath, folderName);
  shareManager._updateAllIncomingShares();

  // now look for a bookmark folder with an incoming-share annotation
  let a = annoSvc.getItemsWithAnnotation("weave/shared-incoming",
		                         {});
  do_check_eq( a.length, 1); // should be just one
  // TODO next look at its children:
   */
}


