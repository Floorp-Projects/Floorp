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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;

const PERMS_FILE      = 0644;
const PERMS_DIRECTORY = 0755;

const STORAGE_FORMAT_VERSION = 2;

const ONE_BYTE = 1;
const ONE_KILOBYTE = 1024 * ONE_BYTE;
const ONE_MEGABYTE = 1024 * ONE_KILOBYTE;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

/*
 * Service object
 * Implements IBookmarksSyncService, main entry point 
 */

function BookmarksSyncService() { this._init(); }
BookmarksSyncService.prototype = {

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

  __ans: null,
  get _ans() {
    if (!this.__ans)
      this.__ans = Cc["@mozilla.org/browser/annotation-service;1"].
                   getService(Ci.nsIAnnotationService);
    return this.__ans;
  },

  __ts: null,
  get _ts() {
    if (!this.__ts)
      this.__ts = Cc["@mozilla.org/browser/tagging-service;1"].
                  getService(Ci.nsITaggingService);
    return this.__ts;
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

  __os: null,
  get _os() {
    if (!this.__os)
      this.__os = Cc["@mozilla.org/observer-service;1"]
        .getService(Ci.nsIObserverService);
    return this.__os;
  },

  __dirSvc: null,
  get _dirSvc() {
    if (!this.__dirSvc)
      this.__dirSvc = Cc["@mozilla.org/file/directory_service;1"].
        getService(Ci.nsIProperties);
    return this.__dirSvc;
  },

  __dav: null,
  get _dav() {
    if (!this.__dav)
      this.__dav = new DAVCollection();
    return this.__dav;
  },

  __se: null,
  get _se() {
    if (!this.__se)
      this.__se = new BookmarksSyncEngine();
    return this.__se;
  },

  // Logger object
  _log: null,

  // Timer object for automagically syncing
  _scheduleTimer: null,

  __encrypter: {},
  __encrypterLoaded: false,
  get _encrypter() {
    if (!this.__encrypterLoaded) {
      let jsLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
        getService(Ci.mozIJSSubScriptLoader);
      jsLoader.loadSubScript("chrome://weave/content/encrypt.js", this.__encrypter);
      this.__encrypterLoaded = true;
    }
    return this.__encrypter;
  },

  // Last synced tree, version, and GUID (to detect if the store has
  // been completely replaced and invalidate the snapshot)
  _snapshot: {},
  _snapshotVersion: 0,

  __snapshotGUID: null,
  get _snapshotGUID() {
    if (!this.__snapshotGUID) {
      let uuidgen = Cc["@mozilla.org/uuid-generator;1"].
        getService(Ci.nsIUUIDGenerator);
      this.__snapshotGUID = uuidgen.generateUUID().toString().replace(/[{}]/g, '');
    }
    return this.__snapshotGUID;
  },
  set _snapshotGUID(GUID) {
    this.__snapshotGUID = GUID;
  },

  get username() {
    let branch = Cc["@mozilla.org/preferences-service;1"]
      .getService(Ci.nsIPrefBranch);
    return branch.getCharPref("browser.places.sync.username");
  },
  set username(value) {
    let branch = Cc["@mozilla.org/preferences-service;1"]
      .getService(Ci.nsIPrefBranch);
    return branch.setCharPref("browser.places.sync.username", value);
  },

  _lmGet: function BSS__lmGet(realm) {
    // fixme: make a request and get the realm
    let password;
    let lm = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
    let logins = lm.findLogins({}, 'chrome://sync', null, realm);

    for (let i = 0; i < logins.length; i++) {
      if (logins[i].username == this.username) {
        password = logins[i].password;
        break;
      }
    }
    return password;
  },

  _lmSet: function BSS__lmSet(realm, password) {
    // cleanup any existing passwords
    let lm = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
    let logins = lm.findLogins({}, 'chrome://sync', null, realm);
    for(let i = 0; i < logins.length; i++) {
      lm.removeLogin(logins[i]);
    }

    if (!password)
      return;

    // save the new one
    let nsLoginInfo = new Components.Constructor(
      "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo, "init");
    let login = new nsLoginInfo('chrome://sync', null, realm,
                                this.username, password, null, null);
    lm.addLogin(login);
  },

  _password: null,
  get password() {
    return this._lmGet('Mozilla Services Password');
  },
  set password(value) {
    if (this._password === null)
      return this._lmSet('Mozilla Services Password', value);
    return this._password;
  },

  _passphrase: null,
  get passphrase() {
    if (this._passphrase === null)
      return this._lmGet('Mozilla Services Encryption Passphrase');
    return this._passphrase;
  },
  set passphrase(value) {
    this._lmSet('Mozilla Services Encryption Passphrase', value);
  },

  get userPath() {
    this._log.info("Hashing username " + this.username);

    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
      createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    let hasher = Cc["@mozilla.org/security/hash;1"]
      .createInstance(Ci.nsICryptoHash);
    hasher.init(hasher.SHA1);

    let data = converter.convertToByteArray(this.username, {});
    hasher.update(data, data.length);
    let rawHash = hasher.finish(false);

    // return the two-digit hexadecimal code for a byte
    function toHexString(charCode) {
      return ("0" + charCode.toString(16)).slice(-2);
    }

    let hash = [toHexString(rawHash.charCodeAt(i)) for (i in rawHash)].join("");
    this._log.debug("Username hashes to " + hash);
    return hash;
  },

  get currentUser() {
    if (this._dav.loggedIn)
      return this.username;
    return null;
  },

  // FIXME: listen for pref changes
  _encryptionChanged: false,
  get encryption() {
    let branch = Cc["@mozilla.org/preferences-service;1"]
      .getService(Ci.nsIPrefBranch);
    return branch.getCharPref("browser.places.sync.encryption");
  },
  set encryption(value) {
    switch (value) {
    case "XXXTEA":
    case "none":
      let branch = Cc["@mozilla.org/preferences-service;1"]
        .getService(Ci.nsIPrefBranch);
      let cur = branch.getCharPref("browser.places.sync.encryption");
      if (value != cur) {
        this._encryptionChanged = true;
        branch.setCharPref("browser.places.sync.encryption", value);
      }
      break;
    default:
      throw "Invalid encryption value: " + value;
    }
  },

  _init: function BSS__init() {
    this._initLogs();
    this._log.info("Bookmarks Sync Service Initializing");

    this._serverURL = 'https://services.mozilla.com/';
    this._user = '';
    let enabled = false;
    let schedule = 0;
    try {
      let branch = Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch);
      this._serverURL = branch.getCharPref("browser.places.sync.serverURL");
      enabled = branch.getBoolPref("browser.places.sync.enabled");
      schedule = branch.getIntPref("browser.places.sync.schedule");

      branch.addObserver("browser.places.sync", this, false);
    }
    catch (ex) { /* use defaults */ }

    this._readSnapshot();

    if (!enabled) {
      this._log.info("Bookmarks sync disabled");
      return;
    }

    switch (schedule) {
    case 0:
      this._log.info("Bookmarks sync enabled, manual mode");
      break;
    case 1:
      this._log.info("Bookmarks sync enabled, automagic mode");
      this._enableSchedule();
      break;
    default:
      this._log.info("Bookmarks sync enabled");
      this._log.info("Invalid schedule setting: " + schedule);
      break;
    }
  },

  _enableSchedule: function BSS__enableSchedule() {
    this._scheduleTimer = Cc["@mozilla.org/timer;1"].
      createInstance(Ci.nsITimer);
    let listener = new EventListener(bind2(this, this._onSchedule));
    this._scheduleTimer.initWithCallback(listener, 1800000, // 30 min
                                         this._scheduleTimer.TYPE_REPEATING_SLACK);
  },

  _disableSchedule: function BSS__disableSchedule() {
    this._scheduleTimer = null;
  },

  _onSchedule: function BSS__onSchedule() {
    this._log.info("Running scheduled sync");
    this.sync();
  },

  _initLogs: function BSS__initLogs() {
    let logSvc = Cc["@mozilla.org/log4moz/service;1"].
      getService(Ci.ILog4MozService);

    this._log = logSvc.getLogger("Service.Main");

    let formatter = logSvc.newFormatter("basic");
    let root = logSvc.rootLogger;
    root.level = root.LEVEL_DEBUG;

    let capp = logSvc.newAppender("console", formatter);
    capp.level = root.LEVEL_WARN;
    root.addAppender(capp);

    let dapp = logSvc.newAppender("dump", formatter);
    dapp.level = root.LEVEL_ALL;
    root.addAppender(dapp);

    let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties);
    let logFile = dirSvc.get("ProfD", Ci.nsIFile);
    let verboseFile = logFile.clone();
    logFile.append("bm-sync.log");
    logFile.QueryInterface(Ci.nsILocalFile);
    verboseFile.append("bm-sync-verbose.log");
    verboseFile.QueryInterface(Ci.nsILocalFile);

    let fapp = logSvc.newFileAppender("rotating", logFile, formatter);
    fapp.level = root.LEVEL_INFO;
    root.addAppender(fapp);
    let vapp = logSvc.newFileAppender("rotating", verboseFile, formatter);
    vapp.level = root.LEVEL_DEBUG;
    root.addAppender(vapp);
  },

  _saveSnapshot: function BSS__saveSnapshot() {
    this._log.info("Saving snapshot to disk");

    let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties);

    let file = dirSvc.get("ProfD", Ci.nsIFile);
    file.append("bm-sync-snapshot.json");
    file.QueryInterface(Ci.nsILocalFile);

    if (!file.exists())
      file.create(file.NORMAL_FILE_TYPE, PERMS_FILE);

    let fos = Cc["@mozilla.org/network/file-output-stream;1"].
      createInstance(Ci.nsIFileOutputStream);
    let flags = MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE;
    fos.init(file, flags, PERMS_FILE, 0);

    let out = {version: this._snapshotVersion,
               GUID: this._snapshotGUID,
               snapshot: this._snapshot};
    out = uneval(out);
    fos.write(out, out.length);
    fos.close();
  },

  _readSnapshot: function BSS__readSnapshot() {
    let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties);

    let file = dirSvc.get("ProfD", Ci.nsIFile);
    file.append("bm-sync-snapshot.json");

    if (!file.exists())
      return;

    let fis = Cc["@mozilla.org/network/file-input-stream;1"].
      createInstance(Ci.nsIFileInputStream);
    fis.init(file, MODE_RDONLY, PERMS_FILE, 0);
    fis.QueryInterface(Ci.nsILineInputStream);

    let json = "";
    while (fis.available()) {
      let ret = {};
      fis.readLine(ret);
      json += ret.value;
    }
    fis.close();
    json = eval(json);

    if (json && 'snapshot' in json && 'version' in json && 'GUID' in json) {
      this._log.info("Read saved snapshot from disk");
      this._snapshot = json.snapshot;
      this._snapshotVersion = json.version;
      this._snapshotGUID = json.GUID;
    }
  },

  _wrapNode: function BSS__wrapNode(node) {
    var items = {};
    this._wrapNodeInternal(node, items, null, null);
    return items;
  },

  _wrapNodeInternal: function BSS__wrapNodeInternal(node, items, parentGUID, index) {
    let GUID = this._bms.getItemGUID(node.itemId);
    let item = {parentGUID: parentGUID,
                index: index};

    if (node.type == node.RESULT_TYPE_FOLDER) {
      if (this._ls.isLivemark(node.itemId)) {
        item.type = "livemark";
        let siteURI = this._ls.getSiteURI(node.itemId);
        let feedURI = this._ls.getFeedURI(node.itemId);
        item.siteURI = siteURI? siteURI.spec : "";
        item.feedURI = feedURI? feedURI.spec : "";
      } else {
        item.type = "folder";
        node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
        node.containerOpen = true;
        for (var i = 0; i < node.childCount; i++) {
          this._wrapNodeInternal(node.getChild(i), items, GUID, i);
        }
      }
      item.title = node.title;
    } else if (node.type == node.RESULT_TYPE_URI ||
               node.type == node.RESULT_TYPE_QUERY) {
      if (this._ms.hasMicrosummary(node.itemId)) {
        item.type = "microsummary";
        let micsum = this._ms.getMicrosummary(node.itemId);
        item.generatorURI = micsum.generator.uri.spec; // breaks local generators
      } else if (node.type == node.RESULT_TYPE_QUERY) {
        item.type = "query";
        item.title = node.title;
      } else {
        item.type = "bookmark";
        item.title = node.title;
      }
      item.URI = node.uri;
      item.tags = this._ts.getTagsForURI(makeURI(node.uri));
      item.keyword = this._bms.getKeywordForBookmark(node.itemId);
    } else if (node.type == node.RESULT_TYPE_SEPARATOR) {
      item.type = "separator";
    } else {
      this._log.warn("Warning: unknown item type, cannot serialize: " + node.type);
      return;
    }

    items[GUID] = item;
  },

  _applyCommandsToObj: function BSS__applyCommandsToObj(commands, obj) {
    for (let i = 0; i < commands.length; i++) {
      this._log.debug("Applying cmd to obj: " + uneval(commands[i]));
      switch (commands[i].action) {
      case "create":
        obj[commands[i].GUID] = eval(uneval(commands[i].data));
        break;
      case "edit":
        if ("GUID" in commands[i].data) {
          // special-case guid changes
          let newGUID = commands[i].data.GUID,
              oldGUID = commands[i].GUID;

          obj[newGUID] = obj[oldGUID];
          delete obj[oldGUID]

          for (let GUID in obj) {
            if (obj[GUID].parentGUID == oldGUID)
              obj[GUID].parentGUID = newGUID;
          }
        }
        for (let prop in commands[i].data) {
          if (prop == "GUID")
            continue;
          obj[commands[i].GUID][prop] = commands[i].data[prop];
        }
        break;
      case "remove":
        delete obj[commands[i].GUID];
        break;
      }
    }
    return obj;
  },

  // Applies commands to the places db
  _applyCommands: function BSS__applyCommands(commandList) {
    for (var i = 0; i < commandList.length; i++) {
      var command = commandList[i];
      this._log.debug("Processing command: " + uneval(command));
      switch (command["action"]) {
      case "create":
        this._createCommand(command);
        break;
      case "remove":
        this._removeCommand(command);
        break;
      case "edit":
        this._editCommand(command);
        break;
      default:
        this._log.error("unknown action in command: " + command["action"]);
        break;
      }
    }
  },

  _createCommand: function BSS__createCommand(command) {
    let newId;
    let parentId = this._bms.getItemIdForGUID(command.data.parentGUID);

    if (parentId < 0) {
      this._log.warn("Creating node with unknown parent -> reparenting to root");
      parentId = this._bms.bookmarksMenuFolder;
    }

    switch (command.data.type) {
    case "query":
    case "bookmark":
    case "microsummary":
      this._log.info(" -> creating bookmark \"" + command.data.title + "\"");
      let URI = makeURI(command.data.URI);
      newId = this._bms.insertBookmark(parentId,
                                       URI,
                                       command.data.index,
                                       command.data.title);
      this._ts.untagURI(URI, null);
      this._ts.tagURI(URI, command.data.tags);
      this._bms.setKeywordForBookmark(newId, command.data.keyword);

      if (command.data.type == "microsummary") {
        this._log.info("   \-> is a microsummary");
        let genURI = makeURI(command.data.generatorURI);
        try {
          let micsum = this._ms.createMicrosummary(URI, genURI);
          this._ms.setMicrosummary(newId, micsum);
        }
        catch(ex) { /* ignore "missing local generator" exceptions */ }
      }
      break;
    case "folder":
      this._log.info(" -> creating folder \"" + command.data.title + "\"");
      newId = this._bms.createFolder(parentId,
                                     command.data.title,
                                     command.data.index);
      break;
    case "livemark":
      this._log.info(" -> creating livemark \"" + command.data.title + "\"");
      newId = this._ls.createLivemark(parentId,
                                      command.data.title,
                                      makeURI(command.data.siteURI),
                                      makeURI(command.data.feedURI),
                                      command.data.index);
      break;
    case "separator":
      this._log.info(" -> creating separator");
      newId = this._bms.insertSeparator(parentId, command.data.index);
      break;
    default:
      this._log.error("_createCommand: Unknown item type: " + command.data.type);
      break;
    }
    if (newId)
      this._bms.setItemGUID(newId, command.GUID);
  },

  _removeCommand: function BSS__removeCommand(command) {
    var itemId = this._bms.getItemIdForGUID(command.GUID);
    if (itemId < 0) {
      this._log.warn("Attempted to remove item " + command.GUID +
                     ", but it does not exist.  Skipping.");
      return;
    }
    var type = this._bms.getItemType(itemId);

    switch (type) {
    case this._bms.TYPE_BOOKMARK:
      this._log.info("  -> removing bookmark " + command.GUID);
      this._bms.removeItem(itemId);
      break;
    case this._bms.TYPE_FOLDER:
      this._log.info("  -> removing folder " + command.GUID);
      this._bms.removeFolder(itemId);
      break;
    case this._bms.TYPE_SEPARATOR:
      this._log.info("  -> removing separator " + command.GUID);
      this._bms.removeItem(itemId);
      break;
    default:
      this._log.error("removeCommand: Unknown item type: " + type);
      break;
    }
  },

  _editCommand: function BSS__editCommand(command) {
    var itemId = this._bms.getItemIdForGUID(command.GUID);
    if (itemId < 0) {
      this._log.warn("Item for GUID " + command.GUID + " not found.  Skipping.");
      return;
    }

    for (let key in command.data) {
      switch (key) {
      case "GUID":
        var existing = this._bms.getItemIdForGUID(command.data.GUID);
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
        this._bms.changeBookmarkURI(itemId, makeURI(command.data.URI));
        break;
      case "index":
        this._bms.moveItem(itemId, this._bms.getFolderIdForItem(itemId),
                           command.data.index);
        break;
      case "parentGUID":
        let index = -1;
        if (command.data.index && command.data.index >= 0)
          index = command.data.index;
        this._bms.moveItem(
          itemId, this._bms.getItemIdForGUID(command.data.parentGUID), index);
        break;
      case "tags":
        let tagsURI = this._bms.getBookmarkURI(itemId);
        this._ts.untagURI(URI, null);
        this._ts.tagURI(tagsURI, command.data.tags);
        break;
      case "keyword":
        this._bms.setKeywordForBookmark(itemId, command.data.keyword);
        break;
      case "generatorURI":
        let micsumURI = makeURI(this._bms.getBookmarkURI(itemId));
        let genURI = makeURI(command.data.generatorURI);
        let micsum = this._ms.createMicrosummary(micsumURI, genURI);
        this._ms.setMicrosummary(itemId, micsum);
        break;
      case "siteURI":
        this._ls.setSiteURI(itemId, makeURI(command.data.siteURI));
        break;
      case "feedURI":
        this._ls.setFeedURI(itemId, makeURI(command.data.feedURI));
        break;
      default:
        this._log.warn("Can't change item property: " + key);
        break;
      }
    }
  },

  _getFolderNodes: function BSS__getFolderNodes(folder) {
    let query = this._hsvc.getNewQuery();
    query.setFolders([folder], 1);
    return this._hsvc.executeQuery(query, this._hsvc.getNewQueryOptions()).root;
  },

  _getWrappedBookmarks: function BSS__getWrappedBookmarks(folder) {
    return this._wrapNode(this._getFolderNodes(folder));
  },

  _getBookmarks: function BSS__getBookmarks() {
    let filed = this._getWrappedBookmarks(this._bms.bookmarksMenuFolder);
    let toolbar = this._getWrappedBookmarks(this._bms.toolbarFolder);
    let unfiled = this._getWrappedBookmarks(this._bms.unfiledBookmarksFolder);

    for (let guid in unfiled) {
      if (!(guid in filed))
        filed[guid] = unfiled[guid];
    }

    for (let guid in toolbar) {
      if (!(guid in filed))
        filed[guid] = toolbar[guid];
    }

    return filed; // (combined)
  },

  _mungeNodes: function BSS__mungeNodes(nodes) {
    let json = uneval(nodes);
    json = json.replace(/:{type/g, ":\n\t{type");
    json = json.replace(/}, /g, "},\n  ");
    json = json.replace(/, parentGUID/g, ",\n\t parentGUID");
    json = json.replace(/, index/g, ",\n\t index");
    json = json.replace(/, title/g, ",\n\t title");
    json = json.replace(/, URI/g, ",\n\t URI");
    json = json.replace(/, tags/g, ",\n\t tags");
    json = json.replace(/, keyword/g, ",\n\t keyword");
    return json;
  },

  _mungeCommands: function BSS__mungeCommands(commands) {
    let json = uneval(commands);
    json = json.replace(/ {action/g, "\n {action");
    //json = json.replace(/, data/g, ",\n  data");
    return json;
  },

  _mungeConflicts: function BSS__mungeConflicts(conflicts) {
    let json = uneval(conflicts);
    json = json.replace(/ {action/g, "\n {action");
    //json = json.replace(/, data/g, ",\n  data");
    return json;
  },

  // IBookmarksSyncService internal implementation

  _lock: function BSS__lock() {
    if (this._locked) {
      this._log.warn("Service lock failed: already locked");
      return false;
    }
    this._locked = true;
    this._log.debug("Service lock acquired");
    return true;
  },

  _unlock: function BSS__unlock() {
    this._locked = false;
    this._log.debug("Service lock released");
  },

  _login: function BSS__login(onComplete) {
    let [self, cont] = yield;
    let success = false;
    let svcLock = this._lock();

    try {
      if (!svcLock)
        return;
      this._log.debug("Logging in");
      this._os.notifyObservers(null, "bookmarks-sync:login-start", "");

      if (!this.username) {
        this._log.warn("No username set, login failed");
        this._os.notifyObservers(null, "bookmarks-sync:login-error", "");
        return;
      }
      if (!this.password) {
        this._log.warn("No password given or found in password manager");
        this._os.notifyObservers(null, "bookmarks-sync:login-error", "");
        return;
      }

      this._dav.baseURL = this._serverURL + "user/" + this.userPath + "/";
      this._log.info("Using server URL: " + this._dav.baseURL);

      this._dav.login.async(this._dav, cont, this.username, this.password);
      success = yield;

    } catch (e) {
      this._log.error("Exception caught: " + e.message);

    } finally {
      this._passphrase = null;
      if (success) {
        this._log.debug("Login successful");
        this._os.notifyObservers(null, "bookmarks-sync:login-end", "");
      } else {
        this._log.debug("Login error");
        this._os.notifyObservers(null, "bookmarks-sync:login-error", "");
      }
      if (svcLock)
        this._unlock();
      generatorDone(this, self, onComplete, success);
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  //       original
  //         / \
  //      A /   \ B
  //       /     \
  // client --C-> server
  //       \     /
  //      D \   / C
  //         \ /
  //        final

  // If we have a saved snapshot, original == snapshot.  Otherwise,
  // it's the empty set {}.

  // C is really the diff between server -> final, so if we determine
  // D we can calculate C from that.  In the case where A and B have
  // no conflicts, C == A and D == B.

  // Sync flow:
  // 1) Fetch server deltas
  // 1.1) Construct current server status from snapshot + server deltas
  // 1.2) Generate single delta from snapshot -> current server status ("B")
  // 2) Generate local deltas from snapshot -> current client status ("A")
  // 3) Reconcile client/server deltas and generate new deltas for them.
  //    Reconciliation won't generate C directly, we will simply diff
  //    server->final after step 3.1.
  // 3.1) Apply local delta with server changes ("D")
  // 3.2) Append server delta to the delta file and upload ("C")

  _doSync: function BSS__doSync(onComplete) {
    let [self, cont] = yield;
    let synced = false;
    let locked = null;
    let svcLock = this._lock();

    try {
      if (!svcLock)
        return;
      this._log.info("Beginning sync");
      this._os.notifyObservers(null, "bookmarks-sync:sync-start", "");

      this._dav.lock.async(this._dav, cont);
      locked = yield;

      if (locked)
        this._log.info("Lock acquired");
      else {
        this._log.warn("Could not acquire lock, aborting sync");
        return;
      }

      // 1) Fetch server deltas
      this._getServerData.async(this, cont);
      let server = yield;

      this._log.info("Local snapshot version: " + this._snapshotVersion);
      this._log.info("Server status: " + server.status);
      this._log.info("Server maxVersion: " + server.maxVersion);
      this._log.info("Server snapVersion: " + server.snapVersion);

      if (server.status != 0) {
        this._log.fatal("Sync error: could not get server status, " +
                        "or initial upload failed.  Aborting sync.");
        return;
      }

      // 2) Generate local deltas from snapshot -> current client status

      let localJson = this._getBookmarks();
      this._se.detectUpdates(cont, this._snapshot, localJson);
      let localUpdates = yield;

      this._log.debug("local json:\n" + this._mungeNodes(localJson));
      this._log.debug("Local updates: " + this._mungeCommands(localUpdates));
      this._log.debug("Server updates: " + this._mungeCommands(server.updates));

      if (server.updates.length == 0 && localUpdates.length == 0) {
        this._snapshotVersion = server.maxVersion;
        this._log.info("Sync complete (1): no changes needed on client or server");
        synced = true;
        return;
      }
	  
      // 3) Reconcile client/server deltas and generate new deltas for them.

      this._log.info("Reconciling client/server updates");
      this._se.reconcile(cont, localUpdates, server.updates);
      let ret = yield;

      let clientChanges = ret.propagations[0];
      let serverChanges = ret.propagations[1];
      let clientConflicts = ret.conflicts[0];
      let serverConflicts = ret.conflicts[1];

      this._log.info("Changes for client: " + clientChanges.length);
      this._log.info("Predicted changes for server: " + serverChanges.length);
      this._log.info("Client conflicts: " + clientConflicts.length);
      this._log.info("Server conflicts: " + serverConflicts.length);
      this._log.debug("Changes for client: " + this._mungeCommands(clientChanges));
      this._log.debug("Predicted changes for server: " + this._mungeCommands(serverChanges));
      this._log.debug("Client conflicts: " + this._mungeConflicts(clientConflicts));
      this._log.debug("Server conflicts: " + this._mungeConflicts(serverConflicts));

      if (!(clientChanges.length || serverChanges.length ||
            clientConflicts.length || serverConflicts.length)) {
        this._log.info("Sync complete (2): no changes needed on client or server");
        this._snapshot = localJson;
        this._snapshotVersion = server.maxVersion;
        this._saveSnapshot();
        synced = true;
        return;
      }

      if (clientConflicts.length || serverConflicts.length) {
        this._log.warn("Conflicts found!  Discarding server changes");
      }

      let savedSnap = eval(uneval(this._snapshot));
      let savedVersion = this._snapshotVersion;
      let newSnapshot;

      // 3.1) Apply server changes to local store
      if (clientChanges.length) {
        this._log.info("Applying changes locally");
        // Note that we need to need to apply client changes to the
        // current tree, not the saved snapshot

        this._snapshot = this._applyCommandsToObj(clientChanges, localJson);
        this._snapshotVersion = server.maxVersion;
        this._applyCommands(clientChanges);
        newSnapshot = this._getBookmarks();

        this._se.detectUpdates(cont, this._snapshot, newSnapshot);
        let diff = yield;
        if (diff.length != 0) {
          this._log.warn("Commands did not apply correctly");
          this._log.debug("Diff from snapshot+commands -> " +
                          "new snapshot after commands:\n" +
                          this._mungeCommands(diff));
          // FIXME: do we really want to revert the snapshot here?
          this._snapshot = eval(uneval(savedSnap));
          this._snapshotVersion = savedVersion;
        }

        this._saveSnapshot();
      }

      // 3.2) Append server delta to the delta file and upload

      // Generate a new diff, from the current server snapshot to the
      // current client snapshot.  In the case where there are no
      // conflicts, it should be the same as what the resolver returned

      newSnapshot = this._getBookmarks();
      this._se.detectUpdates(cont, server.snapshot, newSnapshot);
      let serverDelta = yield;

      // Log an error if not the same
      if (!(serverConflicts.length ||
            deepEquals(serverChanges, serverDelta)))
        this._log.warn("Predicted server changes differ from " +
                       "actual server->client diff (can be ignored in many cases)");

      this._log.info("Actual changes for server: " + serverDelta.length);
      this._log.debug("Actual changes for server: " +
                      this._mungeCommands(serverDelta));

      if (serverDelta.length) {
        this._log.info("Uploading changes to server");

        this._snapshot = newSnapshot;
        this._snapshotVersion = ++server.maxVersion;

        server.deltas.push(serverDelta);

        if (server.formatVersion != STORAGE_FORMAT_VERSION ||
            this._encryptionChanged) {
          this._fullUpload.async(this, cont);
          let status = yield;
          if (!status)
            this._log.error("Could not upload files to server"); // eep?

        } else {
          let data;
          if (this.encryption == "none") {
            data = this._mungeCommands(server.deltas);
          } else if (this.encryption == "XXXTEA") {
            this._log.debug("Encrypting snapshot");
            data = this._encrypter.encrypt(uneval(server.deltas), this.passphrase);
            this._log.debug("Done encrypting snapshot");
          } else {
            this._log.error("Unknown encryption scheme: " + this.encryption);
            return;
          }
          this._dav.PUT("bookmarks-deltas.json", data, cont);
          let deltasPut = yield;

          let c = 0;
          for (GUID in this._snapshot)
            c++;

          this._dav.PUT("bookmarks-status.json",
                        uneval({GUID: this._snapshotGUID,
                                formatVersion: STORAGE_FORMAT_VERSION,
                                snapVersion: server.snapVersion,
                                maxVersion: this._snapshotVersion,
                                snapEncryption: server.snapEncryption,
                                deltasEncryption: this.encryption,
                                bookmarksCount: c}), cont);
          let statusPut = yield;

          if (deltasPut.status >= 200 && deltasPut.status < 300 &&
              statusPut.status >= 200 && statusPut.status < 300) {
            this._log.info("Successfully updated deltas and status on server");
            this._saveSnapshot();
          } else {
            // FIXME: revert snapshot here? - can't, we already applied
            // updates locally! - need to save and retry
            this._log.error("Could not update deltas on server");
          }
        }
      }

      this._log.info("Sync complete");
      synced = true;

    } catch (e) {
      this._log.error("Exception caught: " + e.message);

    } finally {
      let ok = false;
      if (locked) {
        this._dav.unlock.async(this._dav, cont);
        ok = yield;
      }
      if (ok && synced) {
        this._os.notifyObservers(null, "bookmarks-sync:sync-end", "");
        generatorDone(this, self, onComplete, true);
      } else {
        this._os.notifyObservers(null, "bookmarks-sync:sync-error", "");
        generatorDone(this, self, onComplete, false);
      }
      if (svcLock)
        this._unlock();
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  _resetGUIDs: function BSS__resetGUIDs() {
    this._resetGUIDsInt(this._getFolderNodes(this._bms.bookmarksMenuFolder));
    this._resetGUIDsInt(this._getFolderNodes(this._bms.toolbarFolder));
    this._resetGUIDsInt(this._getFolderNodes(this._bms.unfiledBookmarksFolder));
  },

  _resetGUIDsInt: function BSS__resetGUIDsInt(node) {
    if (this._ans.itemHasAnnotation(node.itemId, "placesInternal/GUID"))
      this._ans.removeItemAnnotation(node.itemId, "placesInternal/GUID");

    if (node.type == node.RESULT_TYPE_FOLDER &&
        !this._ls.isLivemark(node.itemId)) {
      node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
      node.containerOpen = true;
      for (var i = 0; i < node.childCount; i++) {
        this._resetGUIDsInt(node.getChild(i));
      }
    }
  },

  _checkStatus: function BSS__checkStatus(code, msg) {
    if (code >= 200 && code < 300)
      return;
    this._log.error(msg + " Error code: " + code);
    throw 'checkStatus failed';
  },

  _decrypt: function BSS__decrypt(alg, data) {
    let out;
    switch (alg) {
    case "XXXTEA":
      try {
        this._log.debug("Decrypting data");
        out = eval(this._encrypter.decrypt(data, this.passphrase));
        this._log.debug("Done decrypting data");
      } catch (e) {
        this._log.error("Could not decrypt server snapshot");
        throw 'decrypt failed';
      }
      break;
    case "none":
      out = eval(data);
      break;
    default:
      this._log.error("Unknown encryption algorithm: " + alg);
      throw 'decrypt failed';
    }
    return out;
  },

  /* Get the deltas/combined updates from the server
   * Returns:
   *   status:
   *     -1: error
   *      0: ok
   * These fields may be null when status is -1:
   *   formatVersion:
   *     version of the data format itself.  For compatibility checks.
   *   maxVersion:
   *     the latest version on the server
   *   snapVersion:
   *     the version of the current snapshot on the server (deltas not applied)
   *   snapEncryption:
   *     encryption algorithm currently used on the server-stored snapshot
   *   deltasEncryption:
   *     encryption algorithm currently used on the server-stored deltas
   *   snapshot:
   *     full snapshot of the latest server version (deltas applied)
   *   deltas:
   *     all of the individual deltas on the server
   *   updates:
   *     the relevant deltas (from our snapshot version to current),
   *     combined into a single set.
   */
  _getServerData: function BSS__getServerData(onComplete) {
    let [self, cont] = yield;
    let ret = {status: -1,
               formatVersion: null, maxVersion: null, snapVersion: null,
               snapEncryption: null, deltasEncryption: null,
               snapshot: null, deltas: null, updates: null};

    try {
      this._log.info("Getting bookmarks status from server");
      this._dav.GET("bookmarks-status.json", cont);
      let resp = yield;
      let status = resp.status;
  
      switch (status) {
      case 200:
        this._log.info("Got bookmarks status from server");
  
        let status = eval(resp.responseText);
        let snap, deltas, allDeltas;
  
        // Bail out if the server has a newer format version than we can parse
        if (status.formatVersion > STORAGE_FORMAT_VERSION) {
          this._log.error("Server uses storage format v" + status.formatVersion +
                    ", this client understands up to v" + STORAGE_FORMAT_VERSION);
          generatorDone(this, self, onComplete, ret)
          return;
        }

        if (status.formatVersion == 0) {
          ret.snapEncryption = status.snapEncryption = "none";
          ret.deltasEncryption = status.deltasEncryption = "none";
        }
  
        if (status.GUID != this._snapshotGUID) {
          this._log.info("Remote/local sync GUIDs do not match.  " +
                      "Forcing initial sync.");
          this._resetGUIDs();
          this._snapshot = {};
          this._snapshotVersion = -1;
          this._snapshotGUID = status.GUID;
        }
  
        if (this._snapshotVersion < status.snapVersion) {
          if (this._snapshotVersion >= 0)
            this._log.info("Local snapshot is out of date");
  
          this._log.info("Downloading server snapshot");
          this._dav.GET("bookmarks-snapshot.json", cont);
          resp = yield;
          this._checkStatus(resp.status, "Could not download snapshot.");
          snap = this._decrypt(status.snapEncryption, resp.responseText);

          this._log.info("Downloading server deltas");
          this._dav.GET("bookmarks-deltas.json", cont);
          resp = yield;
          this._checkStatus(resp.status, "Could not download deltas.");
          allDeltas = this._decrypt(status.deltasEncryption, resp.responseText);
          deltas = eval(uneval(allDeltas));
  
        } else if (this._snapshotVersion >= status.snapVersion &&
                   this._snapshotVersion < status.maxVersion) {
          snap = eval(uneval(this._snapshot));
  
          this._log.info("Downloading server deltas");
          this._dav.GET("bookmarks-deltas.json", cont);
          resp = yield;
          this._checkStatus(resp.status, "Could not download deltas.");
          allDeltas = this._decrypt(status.deltasEncryption, resp.responseText);
          deltas = allDeltas.slice(this._snapshotVersion - status.snapVersion);
  
        } else if (this._snapshotVersion == status.maxVersion) {
          snap = eval(uneval(this._snapshot));
  
          // FIXME: could optimize this case by caching deltas file
          this._log.info("Downloading server deltas");
          this._dav.GET("bookmarks-deltas.json", cont);
          resp = yield;
          this._checkStatus(resp.status, "Could not download deltas.");
          allDeltas = this._decrypt(status.deltasEncryption, resp.responseText);
          deltas = [];
  
        } else { // this._snapshotVersion > status.maxVersion
          this._log.error("Server snapshot is older than local snapshot");
          return;
        }
  
        for (var i = 0; i < deltas.length; i++) {
          snap = this._applyCommandsToObj(deltas[i], snap);
        }
  
        ret.status = 0;
        ret.formatVersion = status.formatVersion;
        ret.maxVersion = status.maxVersion;
        ret.snapVersion = status.snapVersion;
        ret.snapEncryption = status.snapEncryption;
        ret.deltasEncryption = status.deltasEncryption;
        ret.snapshot = snap;
        ret.deltas = allDeltas;
        this._se.detectUpdates(cont, this._snapshot, snap);
        ret.updates = yield;
        break;
  
      case 404:
        this._log.info("Server has no status file, Initial upload to server");
  
        this._snapshot = this._getBookmarks();
        this._snapshotVersion = 0;
        this._snapshotGUID = null; // in case there are other snapshots out there

        this._fullUpload.async(this, cont);
        let uploadStatus = yield;
        if (!uploadStatus)
          return;
  
        this._log.info("Initial upload to server successful");
        this._saveSnapshot();
  
        ret.status = 0;
        ret.formatVersion = STORAGE_FORMAT_VERSION;
        ret.maxVersion = this._snapshotVersion;
        ret.snapVersion = this._snapshotVersion;
        ret.snapEncryption = this.encryption;
        ret.deltasEncryption = this.encryption;
        ret.snapshot = eval(uneval(this._snapshot));
        ret.deltas = [];
        ret.updates = [];
        break;
  
      default:
        this._log.error("Could not get bookmarks.status: unknown HTTP status code " +
                        status);
        break;
      }

    } catch (e) {
      if (e != 'checkStatus failed' &&
          e != 'decrypt failed')
        this._log.error("Exception caught: " + e.message);

    } finally {
      generatorDone(this, self, onComplete, ret)
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  _fullUpload: function BSS__fullUpload(onComplete) {
    let [self, cont] = yield;
    let ret = false;

    try {
      let data;
      if (this.encryption == "none") {
        data = this._mungeNodes(this._snapshot);
      } else if (this.encryption == "XXXTEA") {
        this._log.debug("Encrypting snapshot");
        data = this._encrypter.encrypt(uneval(this._snapshot), this.passphrase);
        this._log.debug("Done encrypting snapshot");
      } else {
        this._log.error("Unknown encryption scheme: " + this.encryption);
        return;
      }
      this._dav.PUT("bookmarks-snapshot.json", data, cont);
      resp = yield;
      this._checkStatus(resp.status, "Could not upload snapshot.");

      this._dav.PUT("bookmarks-deltas.json", uneval([]), cont);
      resp = yield;
      this._checkStatus(resp.status, "Could not upload deltas.");

      let c = 0;
      for (GUID in this._snapshot)
        c++;

      this._dav.PUT("bookmarks-status.json",
                    uneval({GUID: this._snapshotGUID,
                            formatVersion: STORAGE_FORMAT_VERSION,
                            snapVersion: this._snapshotVersion,
                            maxVersion: this._snapshotVersion,
                            snapEncryption: this.encryption,
                            deltasEncryption: "none",
                            bookmarksCount: c}), cont);
      resp = yield;
      this._checkStatus(resp.status, "Could not upload status file.");

      this._log.info("Full upload to server successful");
      ret = true;

    } catch (e) {
      if (e != 'checkStatus failed')
        this._log.error("Exception caught: " + e.message);

    } finally {
      generatorDone(this, self, onComplete, ret)
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  _resetLock: function BSS__resetLock(onComplete) {
    let [self, cont] = yield;
    let success = false;
    let svcLock = this._lock();

    try {
      if (!svcLock)
        return;
      this._log.debug("Resetting server lock");
      this._os.notifyObservers(null, "bookmarks-sync:lock-reset-start", "");

      this._dav.forceUnlock.async(this._dav, cont);
      success = yield;

    } catch (e) {
      this._log.error("Exception caught: " + e.message);

    } finally {
      if (success) {
        this._log.debug("Server lock reset successful");
        this._os.notifyObservers(null, "bookmarks-sync:lock-reset-end", "");
      } else {
        this._log.debug("Server lock reset failed");
        this._os.notifyObservers(null, "bookmarks-sync:lock-reset-error", "");
      }
      if (svcLock)
        this._unlock();
      generatorDone(this, self, onComplete, success);
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  _resetServer: function BSS__resetServer(onComplete) {
    let [self, cont] = yield;
    let done = false;
    let svcLock = this._lock();

    try {
      if (!svcLock)
        return;
      this._log.debug("Resetting server data");
      this._os.notifyObservers(null, "bookmarks-sync:reset-server-start", "");

      this._dav.lock.async(this._dav, cont);
      let locked = yield;
      if (locked)
        this._log.debug("Lock acquired");
      else {
        this._log.warn("Could not acquire lock, aborting server reset");
        return;        
      }

      this._dav.DELETE("bookmarks-status.json", cont);
      let statusResp = yield;
      this._dav.DELETE("bookmarks-snapshot.json", cont);
      let snapshotResp = yield;
      this._dav.DELETE("bookmarks-deltas.json", cont);
      let deltasResp = yield;

      this._dav.unlock.async(this._dav, cont);
      let unlocked = yield;

      function ok(code) {
        if (code >= 200 && code < 300)
          return true;
        if (code == 404)
          return true;
        return false;
      }

      if (!(ok(statusResp.status) && ok(snapshotResp.status) &&
            ok(deltasResp.status))) {
        this._log.error("Could delete server data, response codes " +
                        statusResp.status + ", " + snapshotResp.status + ", " +
                        deltasResp.status);
        return;
      }

      this._log.debug("Server files deleted");
      done = true;
        
    } catch (e) {
      this._log.error("Exception caught: " + e.message);

    } finally {
      if (done) {
        this._log.debug("Server reset completed successfully");
        this._os.notifyObservers(null, "bookmarks-sync:reset-server-end", "");
      } else {
        this._log.debug("Server reset failed");
        this._os.notifyObservers(null, "bookmarks-sync:reset-server-error", "");
      }
      if (svcLock)
        this._unlock();
      generatorDone(this, self, onComplete, done)
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  _resetClient: function BSS__resetClient(onComplete) {
    let [self, cont] = yield;
    let done = false;
    let svcLock = this._lock();

    try {
      if (!svcLock)
        return;
      this._log.debug("Resetting client state");
      this._os.notifyObservers(null, "bookmarks-sync:reset-client-start", "");

      this._snapshot = {};
      this._snapshotVersion = -1;
      this._saveSnapshot();
      done = true;

    } catch (e) {
      this._log.error("Exception caught: " + e.message);

    } finally {
      if (done) {
        this._log.debug("Client reset completed successfully");
        this._os.notifyObservers(null, "bookmarks-sync:reset-client-end", "");
      } else {
        this._log.debug("Client reset failed");
        this._os.notifyObservers(null, "bookmarks-sync:reset-client-error", "");
      }
      if (svcLock)
        this._unlock();
      generatorDone(this, self, onComplete, done);
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  // XPCOM registration
  classDescription: "Bookmarks Sync Service",
  contractID: "@mozilla.org/places/sync-service;1",
  classID: Components.ID("{efb3ba58-69bc-42d5-a430-0746fa4b1a7f}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.IBookmarksSyncService,
                                         Ci.nsIObserver,
                                         Ci.nsISupports]),

  // nsIObserver

  observe: function BSS__observe(subject, topic, data) {
    switch (topic) {
    case "browser.places.sync.enabled":
      switch (data) {
      case false:
        this._log.info("Disabling automagic bookmarks sync");
        this._disableSchedule();
        break;
      case true:
        this._log.info("Enabling automagic bookmarks sync");
        this._enableSchedule();
        break;
      }
      break;
    case "browser.places.sync.schedule":
      switch (data) {
      case 0:
        this._log.info("Disabling automagic bookmarks sync");
        this._disableSchedule();
        break;
      case 1:
        this._log.info("Enabling automagic bookmarks sync");
        this._enableSchedule();
        break;
      default:
        this._log.warn("Unknown schedule value set");
        break;
      }
      break;
    default:
      // ignore, there are prefs we observe but don't care about
    }
  },

  // IBookmarksSyncService public methods

  login: function BSS_login(password, passphrase) {
    // cache password & passphrase
    // if null, _login() will try to get them from the pw manager
    this._password = password;
    this._passphrase = passphrase;
    this._login.async(this);
  },

  logout: function BSS_logout() {
    this._log.info("Logging out");
    this._dav.logout();
    this._passphrase = null;
    this._os.notifyObservers(null, "bookmarks-sync:logout", "");
  },

  sync: function BSS_sync() {
    this._doSync.async(this);
  },

  resetLock: function BSS_resetLock() {
    this._resetLock.async(this);
  },

  resetServer: function BSS_resetServer() {
    this._resetServer.async(this);
  },

  resetClient: function BSS_resetClient() {
    this._resetClient.async(this);
  }
};

/*
 * SyncEngine objects
 * Sync engines deal with diff creation and conflict resolution.
 * Tree data structures where all nodes have GUIDs only need to be
 * subclassed for each data type to implement commandLike and
 * itemExists.
 */

function SyncEngine() {
  this._init();
}
SyncEngine.prototype = {
  _logName: "Sync",

  _init: function SE__init() {
    let logSvc = Cc["@mozilla.org/log4moz/service;1"].
      getService(Ci.ILog4MozService);
    this._log = logSvc.getLogger("Service." + this._logName);
  },

  // FIXME: this won't work for deep objects, or objects with optional properties
  _getEdits: function SE__getEdits(a, b) {
    let ret = {numProps: 0, props: {}};
    for (prop in a) {
      if (!deepEquals(a[prop], b[prop])) {
        ret.numProps++;
        ret.props[prop] = b[prop];
      }
    }
    return ret;
  },

  _nodeParents: function SE__nodeParents(GUID, tree) {
    return this._nodeParentsInt(GUID, tree, []);
  },

  _nodeParentsInt: function SE__nodeParentsInt(GUID, tree, parents) {
    if (!tree[GUID] || !tree[GUID].parentGUID)
      return parents;
    parents.push(tree[GUID].parentGUID);
    return this._nodeParentsInt(tree[GUID].parentGUID, tree, parents);
  },

  _detectUpdates: function SE__detectUpdates(onComplete, a, b) {
    let [self, cont] = yield;
    let listener = new EventListener(cont);
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

    let cmds = [];

    try {
      for (let GUID in a) {

        timer.initWithCallback(listener, 0, timer.TYPE_ONE_SHOT);
        yield; // Yield to main loop

        if (GUID in b) {
          let edits = this._getEdits(a[GUID], b[GUID]);
          if (edits.numProps == 0) // no changes - skip
            continue;
          let parents = this._nodeParents(GUID, b);
          cmds.push({action: "edit", GUID: GUID,
                     depth: parents.length, parents: parents,
                     data: edits.props});
        } else {
          let parents = this._nodeParents(GUID, a); // ???
          cmds.push({action: "remove", GUID: GUID,
                     depth: parents.length, parents: parents});
        }
      }
      for (let GUID in b) {

        timer.initWithCallback(listener, 0, timer.TYPE_ONE_SHOT);
        yield; // Yield to main loop

        if (GUID in a)
          continue;
        let parents = this._nodeParents(GUID, b);
        cmds.push({action: "create", GUID: GUID,
                   depth: parents.length, parents: parents,
                   data: b[GUID]});
      }
      cmds.sort(function(a, b) {
        if (a.depth > b.depth)
          return 1;
        if (a.depth < b.depth)
          return -1;
        if (a.index > b.index)
          return -1;
        if (a.index < b.index)
          return 1;
        return 0; // should never happen, but not a big deal if it does
      });

    } catch (e) {
      this._log.error("Exception caught: " + e.message);

    } finally {
      timer = null;
      generatorDone(this, self, onComplete, cmds);
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  _commandLike: function SE__commandLike(a, b) {
    // Check that neither command is null, and verify that the GUIDs
    // are different (otherwise we need to check for edits)
    if (!a || !b || a.GUID == b.GUID)
      return false;

    // Check that all other properties are the same
    // FIXME: could be optimized...
    for (let key in a) {
      if (key != "GUID" && !deepEquals(a[key], b[key]))
        return false;
    }
    for (let key in b) {
      if (key != "GUID" && !deepEquals(a[key], b[key]))
        return false;
    }
    return true;
  },

  // When we change the GUID of a local item (because we detect it as
  // being the same item as a remote one), we need to fix any other
  // local items that have it as their parent
  _fixParents: function SE__fixParents(list, oldGUID, newGUID) {
    for (let i = 0; i < list.length; i++) {
      if (!list[i])
        continue;
      if (list[i].data.parentGUID == oldGUID)
        list[i].data.parentGUID = newGUID;
      for (let j = 0; j < list[i].parents.length; j++) {
        if (list[i].parents[j] == oldGUID)
          list[i].parents[j] = newGUID;
      }
    }
  },

  _conflicts: function SE__conflicts(a, b) {
    if ((a.GUID == b.GUID) && !deepEquals(a, b))
      return true;
    return false;
  },

  _getPropagations: function SE__getPropagations(commands, conflicts, propagations) {
    for (let i = 0; i < commands.length; i++) {
      let alsoConflicts = function(elt) {
        return (elt.action == "create" || elt.action == "remove") &&
          commands[i].parents.indexOf(elt.GUID) >= 0;
      };
      if (conflicts.some(alsoConflicts))
        conflicts.push(commands[i]);

      let cmdConflicts = function(elt) {
        return elt.GUID == commands[i].GUID;
      };
      if (!conflicts.some(cmdConflicts))
        propagations.push(commands[i]);
    }
  },

  _itemExists: function SE__itemExists(GUID) {
    this._log.error("itemExists needs to be subclassed");
    return false;
  },

  _reconcile: function SE__reconcile(onComplete, listA, listB) {
    let [self, cont] = yield;
    let listener = new EventListener(cont);
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

    let propagations = [[], []];
    let conflicts = [[], []];
    let ret = {propagations: propagations, conflicts: conflicts};

    try {
      for (let i = 0; i < listA.length; i++) {
        for (let j = 0; j < listB.length; j++) {

          timer.initWithCallback(listener, 0, timer.TYPE_ONE_SHOT);
          yield; // Yield to main loop
  
          if (deepEquals(listA[i], listB[j])) {
            delete listA[i];
            delete listB[j];
          } else if (this._commandLike(listA[i], listB[j])) {
            this._fixParents(listA, listA[i].GUID, listB[j].GUID);
            listB[j].data = {GUID: listB[j].GUID};
            listB[j].GUID = listA[i].GUID;
            listB[j].action = "edit";
            delete listA[i];
          }
  
          // watch out for create commands with GUIDs that already exist
          if (listB[j] && listB[j].action == "create" &&
              this._itemExists(listB[j].GUID)) {
            this._log.error("Remote command has GUID that already exists " +
                            "locally. Dropping command.");
            delete listB[j];
          }
        }
      }
  
      listA = listA.filter(function(elt) { return elt });
      listB = listB.filter(function(elt) { return elt });
  
      for (let i = 0; i < listA.length; i++) {
        for (let j = 0; j < listB.length; j++) {

          timer.initWithCallback(listener, 0, timer.TYPE_ONE_SHOT);
          yield; // Yield to main loop
  
          if (this._conflicts(listA[i], listB[j]) ||
              this._conflicts(listB[j], listA[i])) {
            if (!conflicts[0].some(
              function(elt) { return elt.GUID == listA[i].GUID }))
              conflicts[0].push(listA[i]);
            if (!conflicts[1].some(
              function(elt) { return elt.GUID == listB[j].GUID }))
              conflicts[1].push(listB[j]);
          }
        }
      }
  
      this._getPropagations(listA, conflicts[0], propagations[1]);
  
      timer.initWithCallback(listener, 0, timer.TYPE_ONE_SHOT);
      yield; // Yield to main loop
  
      this._getPropagations(listB, conflicts[1], propagations[0]);
      ret = {propagations: propagations, conflicts: conflicts};

    } catch (e) {
      this._log.error("Exception caught: " + e.message);

    } finally {
      timer = null;
      generatorDone(this, self, onComplete, ret);
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  // Public methods

  detectUpdates: function SE_detectUpdates(onComplete, a, b) {
    return this._detectUpdates.async(this, onComplete, a, b);
  },

  reconcile: function SE_reconcile(onComplete, listA, listB) {
    return this._reconcile.async(this, onComplete, listA, listB);
  }
};

function BookmarksSyncEngine() {
  this._init();
}
BookmarksSyncEngine.prototype = {
  _logName: "BMSync",

  __bms: null,
  get _bms() {
    if (!this.__bms)
      this.__bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                   getService(Ci.nsINavBookmarksService);
    return this.__bms;
  },

  // NOTE: Needs to be subclassed
  _itemExists: function BSE__itemExists(GUID) {
    return this._bms.getItemIdForGUID(GUID) >= 0;
  },

  commandLike: function BCC_commandLike(a, b) {
    // Check that neither command is null, that their actions, types,
    // and parents are the same, and that they don't have the same
    // GUID.
    // Items with the same GUID do not qualify for 'likeness' because
    // we already consider them to be the same object, and therefore
    // we need to process any edits.
    // The parent GUID check works because reconcile() fixes up the
    // parent GUIDs as it runs, and the command list is sorted by
    // depth
    if (!a || !b ||
       a.action != b.action ||
       a.data.type != b.data.type ||
       a.data.parentGUID != b.data.parentGUID ||
       a.GUID == b.GUID)
      return false;

    // Bookmarks are allowed to be in a different index as long as
    // they are in the same folder.  Folders and separators must be at
    // the same index to qualify for 'likeness'.
    switch (a.data.type) {
    case "bookmark":
      if (a.data.URI == b.data.URI &&
          a.data.title == b.data.title)
        return true;
      return false;
    case "query":
      if (a.data.URI == b.data.URI &&
          a.data.title == b.data.title)
        return true;
      return false;
    case "microsummary":
      if (a.data.URI == b.data.URI &&
          a.data.generatorURI == b.data.generatorURI)
        return true;
      return false;
    case "folder":
      if (a.index == b.index &&
          a.data.title == b.data.title)
        return true;
      return false;
    case "livemark":
      if (a.data.title == b.data.title &&
          a.data.siteURI == b.data.siteURI &&
          a.data.feedURI == b.data.feedURI)
        return true;
      return false;
    case "separator":
      if (a.index == b.index)
        return true;
      return false;
    default:
      this._log.error("commandLike: Unknown item type: " + uneval(a));
      return false;
    }
  }
};
BookmarksSyncEngine.prototype.__proto__ = new SyncEngine();

/*
 * DAV object
 * Abstracts the raw DAV commands
 */

function DAVCollection(baseURL) {
  this._baseURL = baseURL;
  this._authProvider = new DummyAuthProvider();
  let logSvc = Cc["@mozilla.org/log4moz/service;1"].
    getService(Ci.ILog4MozService);
  this._log = logSvc.getLogger("Service.DAV");
}
DAVCollection.prototype = {
  __dp: null,
  get _dp() {
    if (!this.__dp)
      this.__dp = Cc["@mozilla.org/xmlextras/domparser;1"].
        createInstance(Ci.nsIDOMParser);
    return this.__dp;
  },

  _auth: null,

  get baseURL() {
    return this._baseURL;
  },
  set baseURL(value) {
    this._baseURL = value;
  },

  _loggedIn: false,
  get loggedIn() {
    return this._loggedIn;
  },

  _makeRequest: function DC__makeRequest(onComplete, op, path, headers, data) {
    let [self, cont] = yield;
    let ret;

    try {
      this._log.debug("Creating " + op + " request for " + this._baseURL + path);
  
      let request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance();
      request = request.QueryInterface(Ci.nsIDOMEventTarget);
    
      request.addEventListener("load", new EventListener(cont, "load"), false);
      request.addEventListener("error", new EventListener(cont, "error"), false);
      request = request.QueryInterface(Ci.nsIXMLHttpRequest);
      request.open(op, this._baseURL + path, true);


      // Force cache validation
      let channel = request.channel;
      channel = channel.QueryInterface(Ci.nsIRequest);
      let loadFlags = channel.loadFlags;
      loadFlags |= Ci.nsIRequest.VALIDATE_ALWAYS;
      channel.loadFlags = loadFlags;

      let key;
      for (key in headers) {
        this._log.debug("HTTP Header " + key + ": " + headers[key]);
        request.setRequestHeader(key, headers[key]);
      }
  
      this._authProvider._authFailed = false;
      request.channel.notificationCallbacks = this._authProvider;
  
      request.send(data);
      let event = yield;
      ret = event.target;

      if (this._authProvider._authFailed)
        this._log.warn("_makeRequest: authentication failed");
      if (ret.status < 200 || ret.status >= 300)
        this._log.warn("_makeRequest: got status " + ret.status);

    } catch (e) {
      this._log.error("Exception caught: " + e.message);

    } finally {
      generatorDone(this, self, onComplete, ret);
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  get _defaultHeaders() {
    return {'Authorization': this._auth? this._auth : '',
            'Content-type': 'text/plain',
            'If': this._token?
              "<" + this._baseURL + "> (<" + this._token + ">)" : ''};
  },

  GET: function DC_GET(path, onComplete) {
    return this._makeRequest.async(this, onComplete, "GET", path,
                                   this._defaultHeaders);
  },

  PUT: function DC_PUT(path, data, onComplete) {
    return this._makeRequest.async(this, onComplete, "PUT", path,
                                   this._defaultHeaders, data);
  },

  DELETE: function DC_DELETE(path, onComplete) {
    return this._makeRequest.async(this, onComplete, "DELETE", path,
                                   this._defaultHeaders);
  },

  PROPFIND: function DC_PROPFIND(path, data, onComplete) {
    let headers = {'Content-type': 'text/xml; charset="utf-8"',
                   'Depth': '0'};
    headers.__proto__ = this._defaultHeaders;
    return this._makeRequest.async(this, onComplete, "PROPFIND", path,
                                   headers, data);
  },

  LOCK: function DC_LOCK(path, data, onComplete) {
    let headers = {'Content-type': 'text/xml; charset="utf-8"',
                   'Depth': 'infinity',
                   'Timeout': 'Second-600'};
    headers.__proto__ = this._defaultHeaders;
    return this._makeRequest.async(this, onComplete, "LOCK", path, headers, data);
  },

  UNLOCK: function DC_UNLOCK(path, onComplete) {
    let headers = {'Lock-Token': '<' + this._token + '>'};
    headers.__proto__ = this._defaultHeaders;
    return this._makeRequest.async(this, onComplete, "UNLOCK", path, headers);
  },

  // Login / Logout

  login: function DC_login(onComplete, username, password) {
    let [self, cont] = yield;

    try {
      if (this._loggedIn) {
        this._log.debug("Login requested, but already logged in");
        return;
      }
   
      this._log.info("Logging in");

      let URI = makeURI(this._baseURL);
      this._auth = "Basic " + btoa(username + ":" + password);

      // Make a call to make sure it's working
      this.GET("", cont);
      let resp = yield;

      if (this._authProvider._authFailed || resp.status < 200 || resp.status >= 300)
        return;
  
      this._loggedIn = true;

    } catch (e) {
      this._log.error("Exception caught: " + e.message);

    } finally {
      if (this._loggedIn)
        this._log.info("Logged in");
      else
        this._log.warn("Could not log in");
      generatorDone(this, self, onComplete, this._loggedIn);
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  logout: function DC_logout() {
    this._log.debug("Logging out (forgetting auth header)");
    this._loggedIn = false;
    this.__auth = null;
  },

  // Locking

  _getActiveLock: function DC__getActiveLock(onComplete) {
    let [self, cont] = yield;
    let ret = null;

    try {
      this._log.info("Getting active lock token");
      this.PROPFIND("",
                    "<?xml version=\"1.0\" encoding=\"utf-8\" ?>" +
                    "<D:propfind xmlns:D='DAV:'>" +
                    "  <D:prop><D:lockdiscovery/></D:prop>" +
                    "</D:propfind>", cont);
      let resp = yield;

      if (this._authProvider._authFailed || resp.status < 200 || resp.status >= 300)
        return;

      let tokens = xpath(resp.responseXML, '//D:locktoken/D:href');
      let token = tokens.iterateNext();
      ret = token.textContent;

    } catch (e) {
      this._log.error("Exception caught: " + e.message);

    } finally {
      if (ret)
        this._log.debug("Found an active lock token");
      else
        this._log.debug("No active lock token found");
      generatorDone(this, self, onComplete, ret);
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  lock: function DC_lock(onComplete) {
    let [self, cont] = yield;
    this._token = null;

    try {
      this._log.info("Acquiring lock");

      if (this._token) {
        this._log.debug("Lock called, but we already hold a token");
        return;
      }

      this.LOCK("",
                "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n" +
                "<D:lockinfo xmlns:D=\"DAV:\">\n" +
                "  <D:locktype><D:write/></D:locktype>\n" +
                "  <D:lockscope><D:exclusive/></D:lockscope>\n" +
                "</D:lockinfo>", cont);
      let resp = yield;

      if (this._authProvider._authFailed || resp.status < 200 || resp.status >= 300)
        return;

      let tokens = xpath(resp.responseXML, '//D:locktoken/D:href');
      let token = tokens.iterateNext();
      if (token)
        this._token = token.textContent;

    } catch (e){
      this._log.error("Exception caught: " + e.message);

    } finally {
      if (this._token)
        this._log.info("Lock acquired");
      else
        this._log.warn("Could not acquire lock");
      generatorDone(this, self, onComplete, this._token);
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  unlock: function DC_unlock(onComplete) {
    let [self, cont] = yield;
    try {
      this._log.info("Releasing lock");

      if (this._token === null) {
        this._log.debug("Unlock called, but we don't hold a token right now");
        return;
      }

      this.UNLOCK("", cont);
      let resp = yield;

      if (this._authProvider._authFailed || resp.status < 200 || resp.status >= 300)
        return;

      this._token = null;

    } catch (e){
      this._log.error("Exception caught: " + e.message);

    } finally {
      if (this._token) {
        this._log.info("Could not release lock");
        generatorDone(this, self, onComplete, false);
      } else {
        this._log.info("Lock released (or we didn't have one)");
        generatorDone(this, self, onComplete, true);
      }
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  forceUnlock: function DC_forceUnlock(onComplete) {
    let [self, cont] = yield;
    let unlocked = true;

    try {
      this._log.info("Forcibly releasing any server locks");

      this._getActiveLock.async(this, cont);
      this._token = yield;

      if (!this._token) {
        this._log.info("No server lock found");
        return;
      }

      this._log.info("Server lock found, unlocking");
      this.unlock.async(this, cont);
      unlocked = yield;

    } catch (e){
      this._log.error("Exception caught: " + e.message);

    } finally {
      if (unlocked)
        this._log.debug("Lock released");
      else
        this._log.debug("No lock released");
      generatorDone(this, self, onComplete, unlocked);
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  stealLock: function DC_stealLock(onComplete) {
    let [self, cont] = yield;
    let stolen = null;

    try {
      this.forceUnlock.async(this, cont);
      let unlocked = yield;

      if (unlocked) {
        this.lock.async(this, cont);
        stolen = yield;
      }

    } catch (e){
      this._log.error("Exception caught: " + e.message);

    } finally {
      generatorDone(this, self, onComplete, stolen);
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  }
};


/*
 * Auth provider object
 * Taken from nsMicrosummaryService.js and massaged slightly
 */

function DummyAuthProvider() {}
DummyAuthProvider.prototype = {
  // Implement notification callback interfaces so we can suppress UI
  // and abort loads for bad SSL certs and HTTP authorization requests.
  
  // Interfaces this component implements.
  interfaces: [Ci.nsIBadCertListener,
               Ci.nsIAuthPromptProvider,
               Ci.nsIAuthPrompt,
               Ci.nsIPrompt,
               Ci.nsIProgressEventSink,
               Ci.nsIInterfaceRequestor,
               Ci.nsISupports],

  // Auth requests appear to succeed when we cancel them (since the server
  // redirects us to a "you're not authorized" page), so we have to set a flag
  // to let the load handler know to treat the load as a failure.
  get _authFailed()         { return this.__authFailed; },
  set _authFailed(newValue) { return this.__authFailed = newValue },

  // nsISupports

  QueryInterface: function DAP_QueryInterface(iid) {
    if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
      throw Cr.NS_ERROR_NO_INTERFACE;

    // nsIAuthPrompt and nsIPrompt need separate implementations because
    // their method signatures conflict.  The other interfaces we implement
    // within DummyAuthProvider itself.
    switch(iid) {
    case Ci.nsIAuthPrompt:
      return this.authPrompt;
    case Ci.nsIPrompt:
      return this.prompt;
    default:
      return this;
    }
  },

  // nsIInterfaceRequestor
  
  getInterface: function DAP_getInterface(iid) {
    return this.QueryInterface(iid);
  },

  // nsIBadCertListener

  // Suppress UI and abort secure loads from servers with bad SSL certificates.
  
  confirmUnknownIssuer: function DAP_confirmUnknownIssuer(socketInfo, cert, certAddType) {
    return false;
  },

  confirmMismatchDomain: function DAP_confirmMismatchDomain(socketInfo, targetURL, cert) {
    return false;
  },

  confirmCertExpired: function DAP_confirmCertExpired(socketInfo, cert) {
    return false;
  },

  notifyCrlNextupdate: function DAP_notifyCrlNextupdate(socketInfo, targetURL, cert) {
  },

  // nsIAuthPromptProvider
  
  getAuthPrompt: function(aPromptReason, aIID) {
    this._authFailed = true;
    throw Cr.NS_ERROR_NOT_AVAILABLE;
  },

  // HTTP always requests nsIAuthPromptProvider first, so it never needs
  // nsIAuthPrompt, but not all channels use nsIAuthPromptProvider, so we
  // implement nsIAuthPrompt too.

  // nsIAuthPrompt

  get authPrompt() {
    var resource = this;
    return {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIPrompt]),
      prompt: function(dialogTitle, text, passwordRealm, savePassword, defaultText, result) {
        resource._authFailed = true;
        return false;
      },
      promptUsernameAndPassword: function(dialogTitle, text, passwordRealm, savePassword, user, pwd) {
        resource._authFailed = true;
        return false;
      },
      promptPassword: function(dialogTitle, text, passwordRealm, savePassword, pwd) {
        resource._authFailed = true;
        return false;
      }
    };
  },

  // nsIPrompt

  get prompt() {
    var resource = this;
    return {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIPrompt]),
      alert: function(dialogTitle, text) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      alertCheck: function(dialogTitle, text, checkMessage, checkValue) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      confirm: function(dialogTitle, text) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      confirmCheck: function(dialogTitle, text, checkMessage, checkValue) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      confirmEx: function(dialogTitle, text, buttonFlags, button0Title, button1Title, button2Title, checkMsg, checkValue) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      prompt: function(dialogTitle, text, value, checkMsg, checkValue) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      promptPassword: function(dialogTitle, text, password, checkMsg, checkValue) {
        resource._authFailed = true;
        return false;
      },
      promptUsernameAndPassword: function(dialogTitle, text, username, password, checkMsg, checkValue) {
        resource._authFailed = true;
        return false;
      },
      select: function(dialogTitle, text, count, selectList, outSelection) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      }
    };
  },

  // nsIProgressEventSink

  onProgress: function DAP_onProgress(aRequest, aContext,
                                      aProgress, aProgressMax) {
  },

  onStatus: function DAP_onStatus(aRequest, aContext,
                                  aStatus, aStatusArg) {
  }
};

/*
 * Event listener object
 * Used to handle XMLHttpRequest and nsITimer callbacks
 */

function EventListener(handler, eventName) {
  this._handler = handler;
  this._eventName = eventName;
  let logSvc = Cc["@mozilla.org/log4moz/service;1"].
    getService(Ci.ILog4MozService);
  this._log = logSvc.getLogger("Service.EventHandler");
}
EventListener.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITimerCallback, Ci.nsISupports]),

  // DOM event listener
  handleEvent: function EL_handleEvent(event) {
    this._log.debug("Handling event " + this._eventName);
    this._handler(event);
  },

  // nsITimerCallback
  notify: function EL_notify(timer) {
    this._log.trace("Timer fired");
    this._handler(timer);
  }
};

/*
 * Utility functions
 */

function deepEquals(a, b) {
  if (!a && !b)
    return true;
  if (!a || !b)
    return false;

  if (typeof(a) != "object" && typeof(b) != "object")
    return a == b;
  if (typeof(a) != "object" || typeof(b) != "object")
    return false;

  for (let key in a) {
    if (typeof(a[key]) == "object") {
      if (!typeof(b[key]) == "object")
        return false;
      if (!deepEquals(a[key], b[key]))
        return false;
    } else {
      if (a[key] != b[key])
        return false;
    }
  }
  return true;
}

function makeFile(path) {
  var file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
  file.initWithPath(path);
  return file;
}

function makeURI(URIString) {
  if (URIString === null || URIString == "")
    return null;
  let ioservice = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);
  return ioservice.newURI(URIString, null, null);
}

function xpath(xmlDoc, xpathString) {
  let root = xmlDoc.ownerDocument == null ?
    xmlDoc.documentElement : xmlDoc.ownerDocument.documentElement
  let nsResolver = xmlDoc.createNSResolver(root);

  return xmlDoc.evaluate(xpathString, xmlDoc, nsResolver,
                         Ci.nsIDOMXPathResult.ANY_TYPE, null);
}

function bind2(object, method) {
  return function innerBind() { return method.apply(object, arguments); }
}

Function.prototype.async = function(self, extra_args) {
  try {
    let args = Array.prototype.slice.call(arguments, 1);
    let gen = this.apply(self, args);
    gen.next(); // must initialize before sending
    gen.send([gen, function(data) {continueGenerator(gen, data);}]);
    return gen;
  } catch (e) {
    if (e instanceof StopIteration) {
      dump("async warning: generator stopped unexpectedly");
      return null;
    } else {
      this._log.error("Exception caught: " + e.message);
    }
  }
}

function continueGenerator(generator, data) {
  try { generator.send(data); }
  catch (e) {
    if (e instanceof StopIteration)
      dump("continueGenerator warning: generator stopped unexpectedly");
    else
      dump("Exception caught: " + e.message);
  }
}

// generators created using Function.async can't simply call the
// callback with the return value, since that would cause the calling
// function to end up running (after the yield) from inside the
// generator.  Instead, generators can call this method which sets up
// a timer to call the callback from a timer (and cleans up the timer
// to avoid leaks).  It also closes generators after the timeout, to
// keep things clean.
function generatorDone(object, generator, callback, retval) {
  if (object._timer)
    throw "Called generatorDone when there is a timer already set."

  let cb = bind2(object, function(event) {
    generator.close();
    generator = null;
    object._timer = null;
    if (callback)
      callback(retval);
  });

  object._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  object._timer.initWithCallback(new EventListener(cb),
                                 0, object._timer.TYPE_ONE_SHOT);
}

function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule([BookmarksSyncService]);
}

