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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

const EXPORTED_SYMBOLS = ['CookieEngine', 'CookieTracker', 'CookieStore'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/syncCores.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");

Function.prototype.async = Async.sugar;

function CookieEngine(pbeId) {
  this._init(pbeId);
}
CookieEngine.prototype = {
  __proto__: new SyncEngine(),

  get name() { return "cookies"; },
  get displayName() { return "Cookies"; },
  get logName() { return "CookieEngine"; },
  get serverPrefix() { return "user-data/cookies/"; },

  __store: null,
  get _store() {
    if (!this.__store)
      this.__store = new CookieStore();
    return this.__store;
  },

  __core: null,
  get _core() {
    if (!this.__core)
      this.__core = new CookieSyncCore(this._store);
    return this.__core;
  },

  __tracker: null,
  get _tracker() {
    if (!this.__tracker)
      this.__tracker = new CookieTracker();
    return this.__tracker;
  }
};

function CookieSyncCore(store) {
  this._store = store;
  this._init();
}
CookieSyncCore.prototype = {
  _logName: "CookieSync",
  _store: null,

  _commandLike: function CSC_commandLike(a, b) {
    /* Method required to be overridden.
       a and b each have a .data and a .GUID
       If this function returns true, an editCommand will be
       generated to try to resolve the thing.
       but are a and b objects of the type in the Store or
       are they "commands"?? */
    return false;
  }
};
CookieSyncCore.prototype.__proto__ = new SyncCore();

function CookieStore( cookieManagerStub ) {
  /* If no argument is passed in, this store will query/write to the real
     Mozilla cookie manager component.  This is the normal way to use this
     class in production code.  But for unit-testing purposes, you can pass
     in a stub object that will be used in place of the cookieManager. */
  this._init();
  this._cookieManagerStub = cookieManagerStub;
}
CookieStore.prototype = {
  _logName: "CookieStore",
  _lookup: null,

  // Documentation of the nsICookie interface says:
  // name 	ACString 	The name of the cookie. Read only.
  // value 	ACString 	The cookie value. Read only.
  // isDomain 	boolean 	True if the cookie is a domain cookie, false otherwise. Read only.
  // host 	AUTF8String 	The host (possibly fully qualified) of the cookie. Read only.
  // path 	AUTF8String 	The path pertaining to the cookie. Read only.
  // isSecure 	boolean 	True if the cookie was transmitted over ssl, false otherwise. Read only.
  // expires 	PRUint64 	Expiration time (local timezone) expressed as number of seconds since Jan 1, 1970. Read only.
  // status 	nsCookieStatus 	Holds the P3P status of cookie. Read only.
  // policy 	nsCookiePolicy 	Holds the site's compact policy value. Read only.
  // nsICookie2 deprecates expires, status, and policy, and adds:
  //rawHost 	AUTF8String 	The host (possibly fully qualified) of the cookie without a leading dot to represent if it is a domain cookie. Read only.
  //isSession 	boolean 	True if the cookie is a session cookie. Read only.
  //expiry 	PRInt64 	the actual expiry time of the cookie (where 0 does not represent a session cookie). Read only.
  //isHttpOnly 	boolean 	True if the cookie is an http only cookie. Read only.

  __cookieManager: null,
  get _cookieManager() {
    if ( this._cookieManagerStub != undefined ) {
      return this._cookieManagerStub;
    }
    // otherwise, use the real one
    if (!this.__cookieManager)
      this.__cookieManager = Cc["@mozilla.org/cookiemanager;1"].
                             getService(Ci.nsICookieManager2);
    // need the 2nd revision of the ICookieManager interface
    // because it supports add() and the 1st one doesn't.
    return this.__cookieManager;
  },

  _createCommand: function CookieStore__createCommand(command) {
    /* we got a command to create a cookie in the local browser
       in order to sync with the server. */

    this._log.debug("CookieStore got create command for: " + command.GUID);

    // this assumes command.data fits the nsICookie2 interface
    if ( !command.data.isSession ) {
      // Add only persistent cookies ( not session cookies )
      this._cookieManager.add( command.data.host,
			       command.data.path,
			       command.data.name,
			       command.data.value,
			       command.data.isSecure,
			       command.data.isHttpOnly,
			       command.data.isSession,
			       command.data.expiry );
    }
  },

  _removeCommand: function CookieStore__removeCommand(command) {
    /* we got a command to remove a cookie from the local browser
       in order to sync with the server.
       command.data appears to be equivalent to what wrap() puts in
       the JSON dictionary. */

    if (!(command.GUID in this._lookup)) {
      this._log.warn("Warning! Remove command for unknown item: " + command.GUID);
      return;
    }

    this._log.debug("CookieStore got remove command for: " + command.GUID);

    /* I think it goes like this, according to
     http://developer.mozilla.org/en/docs/nsICookieManager
     the last argument is "always block cookies from this domain?"
     and the answer is "no". */
    this._cookieManager.remove(this._lookup[command.GUID].host,
                               this._lookup[command.GUID].name,
                               this._lookup[command.GUID].path,
                               false);
  },

  _editCommand: function CookieStore__editCommand(command) {
    /* we got a command to change a cookie in the local browser
       in order to sync with the server. */

    if (!(command.GUID in this._lookup)) {
      this._log.warn("Warning! Edit command for unknown item: " + command.GUID);
      return;
    }

    this._log.debug("CookieStore got edit command for: " + command.GUID);

    /* Look up the cookie that matches the one in the command: */
    var iter = this._cookieManager.enumerator;
    var matchingCookie = null;
    while (iter.hasMoreElements()){
      let cookie = iter.getNext();
      if (cookie.QueryInterface( Ci.nsICookie2 ) ){
        // see if host:path:name of cookie matches GUID given in command
	let key = cookie.host + ":" + cookie.path + ":" + cookie.name;
	if (key == command.GUID) {
	  matchingCookie = cookie;
	  break;
	}
      }
    }
    // Update values in the cookie:
    for (var key in command.data) {
      // Whatever values command.data has, use them
      matchingCookie[ key ] = command.data[ key ];
    }
    // Remove the old incorrect cookie from the manager:
    this._cookieManager.remove( matchingCookie.host,
				matchingCookie.name,
				matchingCookie.path,
				false );

    // Re-add the new updated cookie:
    if ( !command.data.isSession ) {
      /* ignore single-session cookies, add only persistent cookies.  */
      this._cookieManager.add( matchingCookie.host,
			       matchingCookie.path,
			       matchingCookie.name,
			       matchingCookie.value,
			       matchingCookie.isSecure,
			       matchingCookie.isHttpOnly,
			       matchingCookie.isSession,
			       matchingCookie.expiry );
    }

    // Also, there's an exception raised because
    // this._data[comand.GUID] is undefined
  },

  wrap: function CookieStore_wrap() {
    /* Return contents of this store, as JSON.
       A dictionary of cookies where the keys are GUIDs and the
       values are sub-dictionaries containing all cookie fields. */
    let items = {};
    var iter = this._cookieManager.enumerator;
    while (iter.hasMoreElements()) {
      var cookie = iter.getNext();
      if (cookie.QueryInterface( Ci.nsICookie2 )) {
	      // String used to identify cookies is
	      // host:path:name
	      if ( cookie.isSession ) {
	        /* Skip session-only cookies, sync only persistent cookies. */
	        continue;
	      }

	      let key = cookie.host + ":" + cookie.path + ":" + cookie.name;
	      items[ key ] = { parentid: '',
			       name: cookie.name,
			       value: cookie.value,
			       isDomain: cookie.isDomain,
			       host: cookie.host,
			       path: cookie.path,
			       isSecure: cookie.isSecure,
			       // nsICookie2 values:
			       rawHost: cookie.rawHost,
			       isSession: cookie.isSession,
			       expiry: cookie.expiry,
			       isHttpOnly: cookie.isHttpOnly };

	      /* See http://developer.mozilla.org/en/docs/nsICookie
	         Note: not syncing "expires", "status", or "policy"
	         since they're deprecated. */

      }
    }
    this._lookup = items;
    return items;
  },

  wipe: function CookieStore_wipe() {
    /* Remove everything from the store.  Return nothing.
       TODO are the semantics of this just wiping out an internal
       buffer, or am I supposed to wipe out all cookies from
       the browser itself for reals? */
    this._cookieManager.removeAll();
  },

  _resetGUIDs: function CookieStore__resetGUIDs() {
    let self = yield;
    /* called in the case where remote/local sync GUIDs do not
       match.  We do need to override this, but since we're deriving
       GUIDs from the cookie data itself and not generating them,
       there's basically no way they can get "out of sync" so there's
       nothing to do here. */
  }
};
CookieStore.prototype.__proto__ = new Store();

function CookieTracker() {
  this._init();
}
CookieTracker.prototype = {
  _logName: "CookieTracker",

  _init: function CT__init() {
    this._log = Log4Moz.Service.getLogger("Service." + this._logName);
    this._score = 0;
    /* cookieService can't register observers, but what we CAN do is
       register a general observer with the global observerService
       to watch for the 'cookie-changed' message. */
    let observerService = Cc["@mozilla.org/observer-service;1"].
                          getService(Ci.nsIObserverService);
    observerService.addObserver( this, 'cookie-changed', false );
  },

  // implement observe method to satisfy nsIObserver interface
  observe: function ( aSubject, aTopic, aData ) {
    /* This gets called when any cookie is added, changed, or removed.
       aData will contain a string "added", "changed", etc. to tell us which,
       but for now we can treat them all the same. aSubject is the new
       cookie object itself. */
    var newCookie = aSubject.QueryInterface( Ci.nsICookie2 );
    if ( newCookie ) {
      if ( !newCookie.isSession ) {
	/* Any modification to a persistent cookie is worth
	   10 points out of 100.  Ignore session cookies. */
	this._score += 10;
      }
    }
  }
}
CookieTracker.prototype.__proto__ = new Tracker();
