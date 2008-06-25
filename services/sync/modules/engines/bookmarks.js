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
 * The Original Code is Bookmarks Sync.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
 *  Jono DiCarlo <jdicarlo@mozilla.org>
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

const EXPORTED_SYMBOLS = ['BookmarksEngine'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

// Annotation to use for shared bookmark folders, incoming and outgoing:
const INCOMING_SHARED_ANNO = "weave/shared-incoming";
const OUTGOING_SHARED_ANNO = "weave/shared-outgoing";
const SERVER_PATH_ANNO = "weave/shared-server-path";
// Standard names for shared files on the server
const KEYRING_FILE_NAME = "keyring";
const SHARED_BOOKMARK_FILE_NAME = "shared_bookmarks";

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/dav.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/crypto.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/syncCores.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");
Cu.import("resource://weave/identity.js");

/* LONGTERM TODO: when we start working on the ability to share other types
of data besides bookmarks, the xmppClient instance should be moved to hang
off of Weave.Service instead of hanging off the BookmarksEngine.  But for
now this is the easiest place to deal with it. */
Cu.import("resource://weave/xmpp/xmppClient.js");

Function.prototype.async = Async.sugar;

function BookmarksEngine(pbeId) {
  this._init(pbeId);
}
BookmarksEngine.prototype = {
  get name() { return "bookmarks"; },
  get logName() { return "BmkEngine"; },
  get serverPrefix() { return "user-data/bookmarks/"; },

  __core: null,
  get _core() {
    if (!this.__core)
      this.__core = new BookmarksSyncCore();
    return this.__core;
  },

  __store: null,
  get _store() {
    if (!this.__store)
      this.__store = new BookmarksStore();
    return this.__store;
  },

  __tracker: null,
  get _tracker() {
    if (!this.__tracker)
      this.__tracker = new BookmarksTracker();
    return this.__tracker;
  },

  __annoSvc: null,
  get _annoSvc() {
    if (!this.__anoSvc)
      this.__annoSvc = Cc["@mozilla.org/browser/annotation-service;1"].
        getService(Ci.nsIAnnotationService);
    return this.__annoSvc;
  },

  _init: function BmkEngine__init( pbeId ) {
    this.__proto__.__proto__._init.call( this, pbeId );
    if ( Utils.prefs.getBoolPref( "xmpp.enabled" ) ) {
      dump( "Starting XMPP client for bookmark engine..." );
      this._startXmppClient.async(this);
    }
  },

  _startXmppClient: function BmkEngine__startXmppClient() {
    // To be called asynchronously.
    let self = yield;

    // Get serverUrl and realm of the jabber server from preferences:
    let serverUrl = Utils.prefs.getCharPref( "xmpp.server.url" );
    let realm = Utils.prefs.getCharPref( "xmpp.server.realm" );

    /* Username/password for XMPP are the same; as the ones for Weave,
        so read them from the weave identity: */
    let clientName = ID.get('WeaveID').username;
    let clientPassword = ID.get('WeaveID').password;

    let transport = new HTTPPollingTransport( serverUrl, false, 15000 );
    let auth = new PlainAuthenticator();
    /* LONGTERM TODO would prefer to use MD5Authenticator instead,
        once we get it working, but since we are connecting over SSL, the
        Plain auth is probably fine for now. */
    this._xmppClient = new XmppClient( clientName,
                                       realm,
                                       clientPassword,
				       transport,
                                       auth );
    let bmkEngine = this;
    let messageHandler = {
      handle: function ( messageText, from ) {
        /* The callback function for incoming xmpp messages.
           We expect message text to be in the form of:
	   <command> <serverpath> <foldername>.
	   Where command is one of:
           "share" (sender offers to share directory dir with us)
           or "stop" (sender has stopped sharing directory dir with us.)
           or "accept" (sharee has accepted our offer to share our dir.)
           or "decline" (sharee has declined our offer to share our dir.)

	   Folder name is the human-readable name of the bookmark folder
	   being shared (it can contain spaces).  serverpath is the path
	   on the server to the directory where the data is stored:
	   only the machine seese this, and it can't have spaces.
        */
 	let words = messageText.split(" ");
	let commandWord = words[0];
	let serverPath = words[1];
	let directoryName = words.slice(2).join(" ");
        if ( commandWord == "share" ) {
	  bmkEngine._incomingShareOffer(from, serverPath, folderName);
	} else if ( commandWord == "stop" ) {
	  bmkEngine._incomingShareWithdrawn(from, serverPath, folderName);
	}
      }
    };

    this._xmppClient.registerMessageHandler( messageHandler );
    this._xmppClient.connect( realm, self.cb );
    yield;
    if ( this._xmppClient._connectionStatus == this._xmppClient.FAILED ) {
      this._log.warn( "Weave can't log in to xmpp server: xmpp disabled." );
    } else if ( this._xmppClient._connectionStatus == this._xmppClient.CONNECTED ) {
      this._log.info( "Weave logged into xmpp OK." );
    }
    self.done();
  },

  _incomingShareOffer: function BmkEngine__incomingShareOffer(user,
                                                              serverPath,
                                                              folderName) {
    /* Called when we receive an offer from another user to share a
       folder.

       TODO what should happen is that we add a notification to the queue
       telling that the incoming share has been offered; when the offer
       is accepted we will call createIncomingShare and then
       updateIncomingShare.

       But since we don't have notification in place yet, I'm going to skip
       right ahead to creating the incoming share.
    */
    this._log.info("User " + user + " offered to share folder " + folderName);
    this._createIncomingShare( user, serverPath, folderName );
  },

  _incomingShareWithdrawn: function BmkEngine__incomingShareStop(user,
                                                                 serverPath,
                                                                 folderName) {
    /* Called when we receive a message telling us that a user who has
       already shared a directory with us has chosen to stop sharing
       the directory.
    */
    this._log.info("User " + user + " stopped sharing folder " + folderName);
    this._stopIncomingShare(user, serverPath, folderName);
  },

  _sync: function BmkEngine__sync() {
    /* After syncing, also call syncMounts to get the
       incoming shared bookmark folder contents. */
    let self = yield;
    this.__proto__.__proto__._sync.async(this, self.cb );
    yield;
    this.updateAllOutgoingShares(self.cb);
    yield;
    this.updateAllIncomingShares(self.cb);
    yield;
    self.done();
  },

  _share: function BmkEngine__share( selectedFolder, username ) {
    // Return true if success, false if failure.
    let ret = false;
    let self = yield;

    /* TODO What should the behavior be if i'm already sharing it with user
       A and I ask to share it with user B?  (This should be prevented by
       the UI. */

    // Create the outgoing share folder on the server
    this._createOutgoingShare.async( this, self.cb, selectedFolder, username );
    let serverPath = yield;
    this._updateOutgoingShare.async( this, self.cb, selectedFolder );
    yield;

    /* Set the annotation on the folder so we know
       it's an outgoing share: */
    let folderItemId = selectedFolder.node.itemId;
    let folderName = selectedFolder.getAttribute( "label" );
    this._annoSvc.setItemAnnotation(folderItemId,
                                    OUTGOING_SHARED_ANNO,
                                    username,
                                    0,
                                    this._annoSvc.EXPIRE_NEVER);
    // Send an xmpp message to the share-ee
    if ( this._xmppClient ) {
      if ( this._xmppClient._connectionStatus == this._xmppClient.CONNECTED ) {
	let msgText = "share " + serverPath + " " + folderName;
	this._log.debug( "Sending XMPP message: " + msgText );
	this._xmppClient.sendMessage( username, msgText );
      } else {
	this._log.warn( "No XMPP connection for share notification." );
      }
    }

    /* LONGTERM TODO: in the future when we allow sharing one folder
       with many people, the value of the annotation can be a whole list
       of usernames instead of just one. */

    this._log.info("Shared " + folderName +" with " + username);
    ret = true;
    self.done( ret );
  },

  _stopSharing: function BmkEngine__stopSharing( selectedFolder, username ) {
    let self = yield;
    let folderName = selectedFolder.getAttribute( "label" );
    let serverPath = this._annoSvc.getItemAnnotation(folderNode,
                                                     SERVER_PATH_ANNO);

    /* LONGTERM TODO: when we move to being able to share one folder with
     * multiple people, this needs to be modified so we can stop sharing with
     * one person but keep sharing with others.
     */

    // Stop the outgoing share:
    this._stopOutgoingShare.async( this, self.cb, selectedFolder);
    yield;

    // Send message to the share-ee, so they can stop their incoming share:
    if ( this._xmppClient ) {
      if ( this._xmppClient._connectionStatus == this._xmppClient.CONNECTED ) {
	let msgText = "stop " + serverPath + " " + folderName;
	this._log.debug( "Sending XMPP message: " + msgText );
	this._xmppClient.sendMessage( username, msgText );
      } else {
	this._log.warn( "No XMPP connection for share notification." );
      }
    }

    this._log.info("Stopped sharing " + folderName + "with " + username);
    self.done( true );
  },

  updateAllIncomingShares: function BmkEngine_updateAllIncoming(onComplete) {
    this._updateAllIncomingShares.async(this, onComplete);
  },
  _updateAllIncomingShares: function BmkEngine__updateAllIncoming() {
    /* For every bookmark folder in my tree that has the annotation
       marking it as an incoming shared folder, pull down its latest
       contents from its owner's account on the server.  (This is
       a one-way data transfer because I can't modify bookmarks that
       are owned by someone else but shared to me; any changes I make
       to the folder contents are simply wiped out by the latest
       server contents.) */
    let self = yield;
    let mounts = this._store.findIncomingShares();

    for (let i = 0; i < mounts.length; i++) {
      try {
        this._updateIncomingShare.async(this, self.cb, mounts[i]);
        yield;
      } catch (e) {
        this._log.warn("Could not sync shared folder from " + mounts[i].userid);
        this._log.trace(Utils.stackTrace(e));
      }
    }
  },

  updateAllOutgoingShares: function BmkEngine_updateAllOutgoing(onComplete) {
    this._updateAllOutgoingShares.async(this, onComplete);
  },
  _updateAllOutgoingShares: function BmkEngine__updateAllOutgoing() {
    let self = yield;
    let shares = this._annoSvc.getItemsWithAnnotation(OUTGOING_SHARED_ANNO,
                                                      {});
    for ( let i=0; i < shares.length; i++ ) {
      /* TODO only update the shares that have changed.  Perhaps we can
      do this by checking whether there's a corresponding entry in the
      diff produced by the latest sync. */
      this._updateOutgoingShare.async(this, self.cb, shares[i]);
      yield;
    }
    self.done();
  },

  _createOutgoingShare: function BmkEngine__createOutgoing(folder, username) {
    /* To be called asynchronously.  Folder is a node indicating the bookmark
       folder that is being shared; username is a string indicating the user
       that it is to be shared with.  This function creates the directory and
       keyring on the server in which the shared data will be put, but it
       doesn't actually put the bookmark data there (that's done in
       _updateOutgoingShare().) */
    let self = yield;
    let myUserName = ID.get('WeaveID').username;
    this._log.debug("Sharing bookmarks from " + folder.getAttribute( "label" )
                    + " with " + username);

    /* Generate a new GUID to use as the new directory name on the server
       in which we'll store the shared data. */
    let folderGuid = Utils.makeGUID();

    /* Create the directory on the server if it does not exist already. */
    let serverPath = "/user/" + myUserName + "/share/" + folderGuid;
    DAV.MKCOL(serverPath, self.cb);
    let ret = yield;
    if (!ret) {
      this._log.error("Can't create remote folder for outgoing share.");
      self.done(false);
    }
    // TODO more error handling

    /* Store the path to the server directory in an annotation on the shared
       bookmark folder, so we can easily get back to it later. */
    this._annoSvc.setItemAnnotation(folder.node.itemId,
                                    SERVER_PATH_ANNO,
                                    serverPath,
                                    0,
                                    this._annoSvc.EXPIRE_NEVER);

    // Create a new symmetric key, to be used only for encrypting this share.
    // XXX HACK. Seems like the engine shouldn't have to be doing any of this, or
    // should use its own identity here.
    let tmpIdentity = {
                        realm   : "temp ID",
                        bulkKey : null,
                        bulkIV  : null
                      };
    Crypto.randomKeyGen.async(Crypto, self.cb, tmpIdentity);
    yield;
    let bulkKey = tmpIdentity.bulkKey;
    let bulkIV  = tmpIdentity.bulkIV;

    /* Get public keys for me and the user I'm sharing with.
       Each user's public key is stored in /user/username/public/pubkey. */
    let idRSA = ID.get('WeaveCryptoID');
    let userPubKeyFile = new Resource("/user/" + username + "/public/pubkey");
    userPubKeyFile.get(self.cb);
    let userPubKey = yield;

    /* Create the keyring, containing the sym key encrypted with each
       of our public keys: */
    Crypto.wrapKey.async(Crypto, self.cb, bulkKey, {realm : "tmpWrapID", pubkey: idRSA.pubkey} );
    let encryptedForMe = yield;
    Crypto.wrapKey.async(Crypto, self.cb, bulkKey, {realm : "tmpWrapID", pubkey: userPubKey} );
    let encryptedForYou = yield;
    let keys = {
                 ring   : { },
                 bulkIV : bulkIV
               };
    keys.ring[myUserName] = encryptedForMe;
    keys.ring[username]   = encryptedForYou;

    let keyringFile = new Resource( serverPath + "/" + KEYRING_FILE_NAME );
    keyringFile.put( self.cb, this._json.encode( keys ) );
    yield;

    // Call Atul's js api for setting htaccess:
    let sharingApi = new Sharing.Api( DAV );
    sharingApi.shareWithUsers( serverPath, [username], self.cb );
    let result = yield;

    // return the server path:
    self.done( serverPath );
  },

  _updateOutgoingShare: function BmkEngine__updateOutgoing(folderNode) {
    /* Puts all the bookmark data from the specified bookmark folder,
       encrypted, onto the shared directory on the server (replacing
       anything that was already there).
       To be called asynchronously.
       TODO: error handling*/
    let self = yield;
    let myUserName = ID.get('WeaveID').username;
    // The folder has an annotation specifying the server path to the
    // directory:
    let serverPath = this._annoSvc.getItemAnnotation(folderNode,
                                                     SERVER_PATH_ANNO);
    // TODO the above can throw an exception if the expected anotation isn't
    // there.
    // From that directory, get the keyring file, and from it, the symmetric
    // key that we'll use to encrypt.
    let keyringFile = new Resource(serverPath + "/" + KEYRING_FILE_NAME);
    keyringFile.get(self.cb);
    let keys = yield;

    // Unwrap (decrypt) the key with the user's private key.
    let idRSA = ID.get('WeaveCryptoID');
    let bulkKey = yield Crypto.unwrapKey.async(Crypto, self.cb,
                           keys.ring[myUserName], idRSA);
    let bulkIV = keys.bulkIV;

    // Get the json-wrapped contents of everything in the folder:
    let json = this._store._wrapMount( folderNode, myUserName );
    /* TODO what does wrapMount do with this username?  Should I be passing
       in my own or that of the person I share with? */

    // Encrypt it with the symkey and put it into the shared-bookmark file.
    let bmkFile = new Resource(serverPath + "/" + SHARED_BOOKMARK_FILE_NAME);
    let tmpIdentity = {
                        realm   : "temp ID",
                        bulkKey : bulkKey,
                        bulkIV  : bulkIV
                      };
    Crypto.encryptData.async( Crypto, self.cb, json, tmpIdentity );
    let cyphertext = yield;
    bmkFile.put( self.cb, cyphertext );
    yield;
    self.done();
  },

  _stopOutgoingShare: function BmkEngine__stopOutgoingShare(folderNode) {
    /* Stops sharing the specified folder.  Deletes its data from the
       server, deletes the annotations that mark it as shared, and sends
       a message to the shar-ee to let them know it's been withdrawn. */
    // TODO: currently not called from anywhere.
    let self = yield;
    let serverPath = this._annoSvc.getItemAnnotation( folderNode,
                                                      SERVER_PATH_ANNO );
    let username = this._annoSvc.getItemAnnotation( folderNode,
                                                    OUTGOING_SHARE_ANNO );

    // Delete the share from the server:
    let keyringFile = new Resource(serverPath + "/" + KEYRING_FILE_NAME);
    keyringFile.delete(self.cb);
    yield;
    let bmkFile = new Resource(serverPath + "/" + SHARED_BOOKMARK_FILE_NAME);
    keyringFile.delete(self.cb);
    yield;
    // TODO this leaves the folder itself in place... is there a way to
    // get rid of that, say through DAV?

    // Remove the annotations from the local folder:
    this._annoSvc.setItemAnnotation(folderNode,
                                    SERVER_PATH_ANNO,
                                    "",
                                    0,
                                    this._annoSvc.EXPIRE_NEVER);
    this._annoSvc.setItemAnnotation(folderNode,
                                    OUTGOING_SHARE_ANNO,
                                    "",
                                    0,
                                    this._annoSvc.EXPIRE_NEVER);
    // TODO is there a way to remove the annotations entirely rather than
    // setting it to an empty string??
    self.done();
  },

  _createIncomingShare: function BookmarkEngine__createShare(user,
                                                             serverPath,
                                                             title) {
    /* Creates a new folder in the bookmark menu for the incoming share.
       user is the weave id of the user who is offering the folder to us;
       serverPath is the path on the server where the shared data is located,
       and title is the human-readable title to use for the new folder.

       It is safe to call this again for a folder that already exist: this
       function will exit without doing anything.  It won't create a duplicate.
    */
    let bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
      getService(Ci.nsINavBookmarksService);

    /* Get the toolbar "Shared Folders" folder (identified by its annotation).
       If it doesn't already exist, create it: */
    let root;
    let a = this._annoSvc.getItemsWithAnnotation("weave/mounted-shares-folder",
                                                 {});
    if (a.length == 1)
      root = a[0];
    if (!root) {
      root = bms.createFolder(bms.toolbarFolder, "Shared Folders",
                              bms.DEFAULT_INDEX);
      this._annoSvc.setItemAnnotation(root,
                                      "weave/mounted-shares-folder",
                                      true,
                                      0,
                                      this._annoSvc.EXPIRE_NEVER);
    }
    /* Inside "Shared Folders", create a new folder annotated with
       the originating user and the directory path specified by the incoming
       share offer.  Unless a folder with these exact annotations already
       exists, in which case do nothing. */
    let itemExists = false;
    a = this._annoSvc.getItemsWithAnnotation(INCOMING_SHARED_ANNO, {});
    for (let i = 0; i < a.length; i++) {
      let creator = this._annoSvc.getItemAnnotation(a[i], INCOMING_SHARED_ANNO);
      let path = this._annoSvc.getItemAnnotation(a[i], SERVER_PATH_ANNO);
      if ( creator == user && path == serverPath ) {
        itemExists = true;
        break;
      }
    }
    if (!itemExists) {
      let newId = bms.createFolder(root, title, bms.DEFAULT_INDEX);
      // Keep track of who shared this folder with us...
      this._annoSvc.setItemAnnotation(newId,
                                      INCOMING_SHARED_ANNO,
                                      user,
                                      0,
                                      this._annoSvc.EXPIRE_NEVER);
      // and what the path to the shared data on the server is...
      this._annoSvc.setItemAnnotation(newId,
                                      SERVER_PATH_ANNO,
                                      serverPath,
                                      0,
                                      this._annoSvc.EXPIRE_NEVER);
    }
  },

  _updateIncomingShare: function BmkEngine__updateIncomingShare(mountData) {
    /* Pull down bookmarks from the server for a single incoming
       shared folder, obliterating whatever was in that folder before.

       mountData is an object that's expected to have member data:
       userid: weave id of the user sharing the folder with us,
       rootGUID: guid in our bookmark store of the share mount point,
       node: the bookmark menu node for the share mount point folder,
       snapshot: the json-wrapped current contents of the share. */

    let self = yield;
    let user = mountData.userid;
    let myUserName = ID.get('WeaveID').username;
    // The folder has an annotation specifying the server path to the
    // directory:
    let serverPath = this._annoSvc.getItemAnnotation(mountData.node,
                                                     SERVER_PATH_ANNO);
    // From that directory, get the keyring file, and from it, the symmetric
    // key that we'll use to encrypt.
    let keyringFile = new Resource(serverPath + "/" + KEYRING_FILE_NAME);
    keyringFile.get(self.cb);
    let keys = yield;

    // Decrypt the contents of the bookmark file with the symmetric key:
    let bmkFile = new Resource(serverPath + "/" + SHARED_BOOKMARK_FILE_NAME);
    bmkFile.get(self.cb);
    let cyphertext = yield;
    let tmpIdentity = {
                        realm   : "temp ID",
                        bulkKey : keys.ring[myUserName],
                        bulkIV  : keys.bulkIV
                      };
    Crypto.decryptData.async( Crypto, self.cb, cyphertext, tmpIdentity );
    let json = yield;
    // TODO error handling (see what Resource can throw or return...)

    // prune tree / get what we want
    for (let guid in json) {
      if (json[guid].type != "bookmark")
        delete json[guid];
      else
        json[guid].parentGUID = mountData.rootGUID;
    }

    /* Create diff between the json from server and the current contents;
       then apply the diff. */
    this._log.trace("Got bookmarks from " + user + ", comparing with local copy");
    this._core.detectUpdates(self.cb, mountData.snapshot, snap.data);
    let diff = yield;

    // FIXME: should make sure all GUIDs here live under the mountpoint
    this._log.trace("Applying changes to folder from " + user);
    this._store.applyCommands.async(this._store, self.cb, diff);
    yield;

    this._log.trace("Shared folder from " + user + " successfully synced!");
  },

  _stopIncomingShare: function BmkEngine__stopIncomingShare(user,
                                                            serverPath,
                                                            folderName)
  {
  /* Delete the incoming share folder.  Since the update of incoming folders
   * is triggered when the engine spots a folder with a certain annotation on
   * it, just getting rid of this folder is all we need to do.
   */
    let bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);

    let a = this._annoSvc.getItemsWithAnnotation(OUTGOING_SHARED_ANNO, {});
    for (let i = 0; i < a.length; i++) {
      let creator = this._annoSvc.getItemAnnotation(a[i], OUTGOING_SHARED_ANNO);
      let path = this._annoSvc.getItemAnnotation(a[i], SERVER_PATH_ANNO);
      if ( creator == user && path == serverPath ) {
        bms.removeFolder( a[i]);
      }
    }
  }
};
BookmarksEngine.prototype.__proto__ = new Engine();

function BookmarksSyncCore() {
  this._init();
}
BookmarksSyncCore.prototype = {
  _logName: "BMSync",

  __bms: null,
  get _bms() {
    if (!this.__bms)
      this.__bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                   getService(Ci.nsINavBookmarksService);
    return this.__bms;
  },

  _itemExists: function BSC__itemExists(GUID) {
    return this._bms.getItemIdForGUID(GUID) >= 0;
  },

  _getEdits: function BSC__getEdits(a, b) {
    // NOTE: we do not increment ret.numProps, as that would cause
    // edit commands to always get generated
    let ret = SyncCore.prototype._getEdits.call(this, a, b);
    ret.props.type = a.type;
    return ret;
  },

  // compares properties
  // returns true if the property is not set in either object
  // returns true if the property is set and equal in both objects
  // returns false otherwise
  _comp: function BSC__comp(a, b, prop) {
    return (!a.data[prop] && !b.data[prop]) ||
      (a.data[prop] && b.data[prop] && (a.data[prop] == b.data[prop]));
  },

  _commandLike: function BSC__commandLike(a, b) {
    // Check that neither command is null, that their actions, types,
    // and parents are the same, and that they don't have the same
    // GUID.
    // * Items with the same GUID do not qualify for 'likeness' because
    //   we already consider them to be the same object, and therefore
    //   we need to process any edits.
    // * Remove or edit commands don't qualify for likeness either,
    //   since remove or edit commands with different GUIDs are
    //   guaranteed to refer to two different items
    // * The parent GUID check works because reconcile() fixes up the
    //   parent GUIDs as it runs, and the command list is sorted by
    //   depth
    if (!a || !b ||
        a.action != b.action ||
        a.action != "create" ||
        a.data.type != b.data.type ||
        a.data.parentGUID != b.data.parentGUID ||
        a.GUID == b.GUID)
      return false;

    // Bookmarks and folders are allowed to be in a different index as long as
    // they are in the same folder.  Separators must be at
    // the same index to qualify for 'likeness'.
    switch (a.data.type) {
    case "bookmark":
      if (this._comp(a, b, 'URI') &&
          this._comp(a, b, 'title'))
        return true;
      return false;
    case "query":
      if (this._comp(a, b, 'URI') &&
          this._comp(a, b, 'title'))
        return true;
      return false;
    case "microsummary":
      if (this._comp(a, b, 'URI') &&
          this._comp(a, b, 'generatorURI'))
        return true;
      return false;
    case "folder":
      if (this._comp(a, b, 'title'))
        return true;
      return false;
    case "livemark":
      if (this._comp(a, b, 'title') &&
          this._comp(a, b, 'siteURI') &&
          this._comp(a, b, 'feedURI'))
        return true;
      return false;
    case "separator":
      if (this._comp(a, b, 'index'))
        return true;
      return false;
    default:
      let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
      this._log.error("commandLike: Unknown item type: " + json.encode(a));
      return false;
    }
  }
};
BookmarksSyncCore.prototype.__proto__ = new SyncCore();

function BookmarksStore() {
  this._init();
}
BookmarksStore.prototype = {
  _logName: "BStore",

  __bms: null,
  get _bms() {
    if (!this.__bms)
      this.__bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                   getService(Ci.nsINavBookmarksService);
    return this.__bms;
  },

  __hsvc: null,
  get _hsvc() {
    if (!this.__hsvc)
      this.__hsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                    getService(Ci.nsINavHistoryService);
    return this.__hsvc;
  },

  __ls: null,
  get _ls() {
    if (!this.__ls)
      this.__ls = Cc["@mozilla.org/browser/livemark-service;2"].
        getService(Ci.nsILivemarkService);
    return this.__ls;
  },

  __ms: null,
  get _ms() {
    if (!this.__ms)
      this.__ms = Cc["@mozilla.org/microsummary/service;1"].
        getService(Ci.nsIMicrosummaryService);
    return this.__ms;
  },

  __ts: null,
  get _ts() {
    if (!this.__ts)
      this.__ts = Cc["@mozilla.org/browser/tagging-service;1"].
                  getService(Ci.nsITaggingService);
    return this.__ts;
  },

  __ans: null,
  get _ans() {
    if (!this.__ans)
      this.__ans = Cc["@mozilla.org/browser/annotation-service;1"].
                   getService(Ci.nsIAnnotationService);
    return this.__ans;
  },

  _getItemIdForGUID: function BStore__getItemIdForGUID(GUID) {
    switch (GUID) {
    case "menu":
      return this._bms.bookmarksMenuFolder;
    case "toolbar":
      return this._bms.toolbarFolder;
    case "unfiled":
      return this._bms.unfiledBookmarksFolder;
    default:
      return this._bms.getItemIdForGUID(GUID);
    }
    return null;
  },

  _createCommand: function BStore__createCommand(command) {
    let newId;
    let parentId = this._getItemIdForGUID(command.data.parentGUID);

    if (parentId < 0) {
      this._log.warn("Creating node with unknown parent -> reparenting to root");
      parentId = this._bms.bookmarksMenuFolder;
    }

    switch (command.data.type) {
    case "query":
    case "bookmark":
    case "microsummary": {
      this._log.debug(" -> creating bookmark \"" + command.data.title + "\"");
      let URI = Utils.makeURI(command.data.URI);
      newId = this._bms.insertBookmark(parentId,
                                       URI,
                                       command.data.index,
                                       command.data.title);
      this._ts.untagURI(URI, null);
      this._ts.tagURI(URI, command.data.tags);
      this._bms.setKeywordForBookmark(newId, command.data.keyword);
      if (command.data.description) {
        this._ans.setItemAnnotation(newId, "bookmarkProperties/description",
                                    command.data.description, 0,
                                    this._ans.EXPIRE_NEVER);
      }

      if (command.data.type == "microsummary") {
        this._log.debug("   \-> is a microsummary");
        this._ans.setItemAnnotation(newId, "bookmarks/staticTitle",
                                    command.data.staticTitle || "", 0, this._ans.EXPIRE_NEVER);
        let genURI = Utils.makeURI(command.data.generatorURI);
        try {
          let micsum = this._ms.createMicrosummary(URI, genURI);
          this._ms.setMicrosummary(newId, micsum);
        }
        catch(ex) { /* ignore "missing local generator" exceptions */ }
      }
    } break;
    case "folder":
      this._log.debug(" -> creating folder \"" + command.data.title + "\"");
      newId = this._bms.createFolder(parentId,
                                     command.data.title,
                                     command.data.index);
      // If folder is an outgoing share, put the annotations on it:
      if ( command.data.outgoingShareAnno != undefined ) {
	this._ans.setItemAnnotation(newId,
				    OUTGOING_SHARE_ANNO,
                                    command.data.outgoingShareAnno,
				    0,
				    this._ans.EXPIRE_NEVER);
	this._ans.setItemAnnotation(newId,
				    SERVER_PATH_ANNO,
                                    command.data.serverPathAnno,
				    0,
				    this._ans.EXPIRE_NEVER);

      }
      break;
    case "livemark":
      this._log.debug(" -> creating livemark \"" + command.data.title + "\"");
      newId = this._ls.createLivemark(parentId,
                                      command.data.title,
                                      Utils.makeURI(command.data.siteURI),
                                      Utils.makeURI(command.data.feedURI),
                                      command.data.index);
      break;
    case "incoming-share":
      /* even though incoming shares are folders according to the
       * bookmarkService, _wrap() wraps them as type=incoming-share, so we
       * handle them separately, like so: */
      this._log.debug(" -> creating incoming-share \"" + command.data.title + "\"");
      newId = this._bms.createFolder(parentId,
                                     command.data.title,
                                     command.data.index);
      this._ans.setItemAnnotation(newId, INCOMING_SHARE_ANNO,
                                  command.data.username, 0, this._ans.EXPIRE_NEVER);
      this._ans.setItemAnnotation(newId, SERVER_PATH_ANNO,
                                  command.data.serverPath, 0, this._ans.EXPIRE_NEVER);
      break;
    case "separator":
      this._log.debug(" -> creating separator");
      newId = this._bms.insertSeparator(parentId, command.data.index);
      break;
    default:
      this._log.error("_createCommand: Unknown item type: " + command.data.type);
      break;
    }
    if (newId)
      this._bms.setItemGUID(newId, command.GUID);
  },

  _removeCommand: function BStore__removeCommand(command) {
    if (command.GUID == "menu" ||
        command.GUID == "toolbar" ||
        command.GUID == "unfiled") {
      this._log.warn("Attempted to remove root node (" + command.GUID +
                     ").  Skipping command.");
      return;
    }

    var itemId = this._bms.getItemIdForGUID(command.GUID);
    if (itemId < 0) {
      this._log.warn("Attempted to remove item " + command.GUID +
                     ", but it does not exist.  Skipping.");
      return;
    }
    var type = this._bms.getItemType(itemId);

    switch (type) {
    case this._bms.TYPE_BOOKMARK:
      this._log.debug("  -> removing bookmark " + command.GUID);
      this._bms.removeItem(itemId);
      break;
    case this._bms.TYPE_FOLDER:
      this._log.debug("  -> removing folder " + command.GUID);
      this._bms.removeFolder(itemId);
      break;
    case this._bms.TYPE_SEPARATOR:
      this._log.debug("  -> removing separator " + command.GUID);
      this._bms.removeItem(itemId);
      break;
    default:
      this._log.error("removeCommand: Unknown item type: " + type);
      break;
    }
  },

  _editCommand: function BStore__editCommand(command) {
    if (command.GUID == "menu" ||
        command.GUID == "toolbar" ||
        command.GUID == "unfiled") {
      this._log.warn("Attempted to edit root node (" + command.GUID +
                     ").  Skipping command.");
      return;
    }

    var itemId = this._bms.getItemIdForGUID(command.GUID);
    if (itemId < 0) {
      this._log.warn("Item for GUID " + command.GUID + " not found.  Skipping.");
      return;
    }

    for (let key in command.data) {
      switch (key) {
      case "type":
        // all commands have this to help in reconciliation, but it makes
        // no sense to edit it
        break;
      case "GUID":
        var existing = this._getItemIdForGUID(command.data.GUID);
        if (existing < 0)
          this._bms.setItemGUID(itemId, command.data.GUID);
        else
          this._log.warn("Can't change GUID " + command.GUID +
                         " to " + command.data.GUID + ": GUID already exists.");
        break;
      case "title":
        this._bms.setItemTitle(itemId, command.data.title);
        break;
      case "URI":
        this._bms.changeBookmarkURI(itemId, Utils.makeURI(command.data.URI));
        break;
      case "index":
        this._bms.moveItem(itemId, this._bms.getFolderIdForItem(itemId),
                           command.data.index);
        break;
      case "parentGUID": {
        let index = -1;
        if (command.data.index && command.data.index >= 0)
          index = command.data.index;
        this._bms.moveItem(
          itemId, this._getItemIdForGUID(command.data.parentGUID), index);
      } break;
      case "tags": {
        let tagsURI = this._bms.getBookmarkURI(itemId);
        this._ts.untagURI(tagsURI, null);
        this._ts.tagURI(tagsURI, command.data.tags);
      } break;
      case "keyword":
        this._bms.setKeywordForBookmark(itemId, command.data.keyword);
        break;
      case "description":
        if (command.data.description) {
          this._ans.setItemAnnotation(itemId, "bookmarkProperties/description",
                                      command.data.description, 0,
                                      this._ans.EXPIRE_NEVER);
        }
        break;
      case "generatorURI": {
        let micsumURI = Utils.makeURI(this._bms.getBookmarkURI(itemId));
        let genURI = Utils.makeURI(command.data.generatorURI);
        let micsum = this._ms.createMicrosummary(micsumURI, genURI);
        this._ms.setMicrosummary(itemId, micsum);
      } break;
      case "siteURI":
        this._ls.setSiteURI(itemId, Utils.makeURI(command.data.siteURI));
        break;
      case "feedURI":
        this._ls.setFeedURI(itemId, Utils.makeURI(command.data.feedURI));
        break;
      case "outgoingShareAnno":
	this._ans.setItemAnnotation(itemId, OUTGOING_SHARE_ANNO,
				    command.data.outgoingShareAnno, 0,
				    this._ans.EXPIRE_NEVER);
	break;
      case "incomingShareAnno":
	this._ans.setItemAnnotation(itemId, INCOMING_SHARE_ANNO,
				    command.data.incomingShareAnno, 0,
				    this._ans.EXPIRE_NEVER);
	break;
      case "serverPathAnno":
	this._ans.setItemAnnotation(itemId, SERVER_PATH_ANNO,
				    command.data.serverPathAnno, 0,
				    this._ans.EXPIRE_NEVER);
	break;
      default:
        this._log.warn("Can't change item property: " + key);
        break;
      }
    }
  },

  _getNode: function BSS__getNode(folder) {
    let query = this._hsvc.getNewQuery();
    query.setFolders([folder], 1);
    return this._hsvc.executeQuery(query, this._hsvc.getNewQueryOptions()).root;
  },

  __wrap: function BSS___wrap(node, items, parentGUID, index, guidOverride) {
    let GUID, item;

    // we override the guid for the root items, "menu", "toolbar", etc.
    if (guidOverride) {
      GUID = guidOverride;
      item = {};
    } else {
      GUID = this._bms.getItemGUID(node.itemId);
      item = {parentGUID: parentGUID, index: index};
    }

    if (node.type == node.RESULT_TYPE_FOLDER) {
      if (this._ls.isLivemark(node.itemId)) {
        item.type = "livemark";
        let siteURI = this._ls.getSiteURI(node.itemId);
        let feedURI = this._ls.getFeedURI(node.itemId);
        item.siteURI = siteURI? siteURI.spec : "";
        item.feedURI = feedURI? feedURI.spec : "";
      } else if (this._ans.itemHasAnnotation(node.itemId, INCOMING_SHARE_ANNO)){
	/* When there's an incoming share, we just sync the folder itself
	 and the values of its annotations: NOT any of its contents.  So
	 we'll wrap it as type=incoming-share, not as a "folder". */
	item.type = "incoming-share";
	item.title = node.title;
        item.serverPath = this._ans.getItemAnnotation(node.itemId,
                                                      SERVER_PATH_ANNO);
	item.username = this._ans.getItemAnnotation(node.itemId,
                                                      INCOMING_SHARE_ANNO);
      } else {
        item.type = "folder";
        node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
        node.containerOpen = true;
	// If folder is an outgoing share, wrap its annotations:
	if (this._ans.itemHasAnnotation(node.itemId, OUTGOING_SHARE_ANNO)) {

	  item.serverPathAnno = this._ans.getItemAnnotation(node.itemId,
                                                      SERVER_PATH_ANNO);
	  item.outgoingShareAnno = this._ans.getItemAnnotation(node.itemId,
                                                      OUTGOING_SHARE_ANNO);
	}

        for (var i = 0; i < node.childCount; i++) {
          this.__wrap(node.getChild(i), items, GUID, i);
        }
      }
      if (!guidOverride)
        item.title = node.title; // no titles for root nodes

    } else if (node.type == node.RESULT_TYPE_URI ||
               node.type == node.RESULT_TYPE_QUERY) {
      if (this._ms.hasMicrosummary(node.itemId)) {
        item.type = "microsummary";
        let micsum = this._ms.getMicrosummary(node.itemId);
        item.generatorURI = micsum.generator.uri.spec; // breaks local generators
        item.staticTitle = this._ans.getItemAnnotation(node.itemId, "bookmarks/staticTitle");
      } else if (node.type == node.RESULT_TYPE_QUERY) {
        item.type = "query";
        item.title = node.title;
      } else {
        item.type = "bookmark";
        item.title = node.title;
      }

      try {
        item.description =
          this._ans.getItemAnnotation(node.itemId, "bookmarkProperties/description");
      } catch (e) {
        item.description = undefined;
      }

      item.URI = node.uri;

      // This will throw if makeURI can't make an nsIURI object out of the
      // node.uri string (or return null if node.uri is null), in which case
      // we won't be able to get tags for the bookmark (but we'll still sync
      // the rest of the record).
      let uri;
      try {
        uri = Utils.makeURI(node.uri);
      }
      catch(e) {
        this._log.error("error parsing URI string <" + node.uri + "> " +
                        "for item " + node.itemId + " (" + node.title + "): " +
                        e);
      }

      if (uri)
        item.tags = this._ts.getTagsForURI(uri, {});

      item.keyword = this._bms.getKeywordForBookmark(node.itemId);

    } else if (node.type == node.RESULT_TYPE_SEPARATOR) {
      item.type = "separator";

    } else {
      this._log.warn("Warning: unknown item type, cannot serialize: " + node.type);
      return;
    }

    items[GUID] = item;
  },

  // helper
  _wrap: function BStore__wrap(node, items, rootName) {
    return this.__wrap(node, items, null, null, rootName);
  },

  _wrapMount: function BStore__wrapMount(node, id) {
    if (node.type != node.RESULT_TYPE_FOLDER)
      throw "Trying to wrap a non-folder mounted share";

    let GUID = this._bms.getItemGUID(node.itemId);
    let ret = {rootGUID: GUID, userid: id, snapshot: {}, folderNode: node};

    node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
    node.containerOpen = true;
    for (var i = 0; i < node.childCount; i++) {
      this.__wrap(node.getChild(i), ret.snapshot, GUID, i);
    }

    // remove any share mountpoints
    for (let guid in ret.snapshot) {
      // TODO decide what to do with this...
      if (ret.snapshot[guid].type == "incoming-share")
        delete ret.snapshot[guid];
    }

    return ret;
  },

  _resetGUIDs: function BSS__resetGUIDs(node) {
    if (this._ans.itemHasAnnotation(node.itemId, "placesInternal/GUID"))
      this._ans.removeItemAnnotation(node.itemId, "placesInternal/GUID");

    if (node.type == node.RESULT_TYPE_FOLDER &&
        !this._ls.isLivemark(node.itemId)) {
      node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
      node.containerOpen = true;
      for (var i = 0; i < node.childCount; i++) {
        this._resetGUIDs(node.getChild(i));
      }
    }
  },

  findIncomingShares: function BStore_findIncomingShares() {
    /* Returns list of mount data structures, each of which
       represents one incoming shared-bookmark folder. */
    let ret = [];
    let a = this._ans.getItemsWithAnnotation(INCOMING_SHARED_ANNO, {});
    for (let i = 0; i < a.length; i++) {
      /* The value of the incoming-shared annotation is the id of the
       person who has shared it with us.  Get that value: */
      let id = this._ans.getItemAnnotation(a[i], INCOMING_SHARED_ANNO);
      ret.push(this._wrapMount(this._getNode(a[i]), id));
    }
    return ret;
  },

  wrap: function BStore_wrap() {
    var items = {};
    this._wrap(this._getNode(this._bms.bookmarksMenuFolder), items, "menu");
    this._wrap(this._getNode(this._bms.toolbarFolder), items, "toolbar");
    this._wrap(this._getNode(this._bms.unfiledBookmarksFolder), items, "unfiled");
    return items;
  },

  wipe: function BStore_wipe() {
    this._bms.removeFolderChildren(this._bms.bookmarksMenuFolder);
    this._bms.removeFolderChildren(this._bms.toolbarFolder);
    this._bms.removeFolderChildren(this._bms.unfiledBookmarksFolder);
  },

  resetGUIDs: function BStore_resetGUIDs() {
    this._resetGUIDs(this._getNode(this._bms.bookmarksMenuFolder));
    this._resetGUIDs(this._getNode(this._bms.toolbarFolder));
    this._resetGUIDs(this._getNode(this._bms.unfiledBookmarksFolder));
  }
};
BookmarksStore.prototype.__proto__ = new Store();

/*
 * Tracker objects for each engine may need to subclass the
 * getScore routine, which returns the current 'score' for that
 * engine. How the engine decides to set the score is upto it,
 * as long as the value between 0 and 100 actually corresponds
 * to its urgency to sync.
 *
 * Here's an example BookmarksTracker. We don't subclass getScore
 * because the observer methods take care of updating _score which
 * getScore returns by default.
 */
function BookmarksTracker() {
  this._init();
}
BookmarksTracker.prototype = {
  _logName: "BMTracker",

  /* We don't care about the first three */
  onBeginUpdateBatch: function BMT_onBeginUpdateBatch() {

  },
  onEndUpdateBatch: function BMT_onEndUpdateBatch() {

  },
  onItemVisited: function BMT_onItemVisited() {

  },

  /* Every add or remove is worth 4 points,
   * on the basis that adding or removing 20 bookmarks
   * means its time to sync?
   */
  onItemAdded: function BMT_onEndUpdateBatch() {
    this._score += 4;
  },
  onItemRemoved: function BMT_onItemRemoved() {
    this._score += 4;
  },
  /* Changes are worth 2 points? */
  onItemChanged: function BMT_onItemChanged() {
    this._score += 2;
  },

  _init: function BMT__init() {
    this._log = Log4Moz.Service.getLogger("Service." + this._logName);
    this._score = 0;

    Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
    getService(Ci.nsINavBookmarksService).
    addObserver(this, false);
  }
}
BookmarksTracker.prototype.__proto__ = new Tracker();
