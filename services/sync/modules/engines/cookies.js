const EXPORTED_SYMBOLS = ['CookieEngine', 'CookieTracker', 'CookieStore'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/syncCores.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");

function CookieEngine(pbeId) {
  this._init(pbeId);
}
CookieEngine.prototype = {
  get name() { return "cookies"; },
  get logName() { return "CookieEngine"; },
  get serverPrefix() { return "user-data/cookies/"; },

  __core: null,
  get _core() {
    if (!this.__core)
      this.__core = new CookieSyncCore();
    return this.__core;
  },

  __store: null,
  get _store() {
    if (!this.__store)
      this.__store = new CookieStore();
    return this.__store;
  },

  __tracker: null,
  get _tracker() {
    if (!this.__tracker)
      this.__tracker = new CookieTracker();
    return this.__tracker;
  }
};
CookieEngine.prototype.__proto__ = new Engine();

function CookieSyncCore() {
  this._init();
}
CookieSyncCore.prototype = {
  _logName: "CookieSync",

  __cookieManager: null,
  get _cookieManager() {
    if (!this.__cookieManager)
      this.__cookieManager = Cc["@mozilla.org/cookiemanager;1"].
                             getService(Ci.nsICookieManager2);
    /* need the 2nd revision of the ICookieManager interface
       because it supports add() and the 1st one doesn't. */
    return this.__cookieManager;
  },


  _itemExists: function CSC__itemExists(GUID) {
    /* true if a cookie with the given GUID exists.
       The GUID that we are passed should correspond to the keys
       that we define in the JSON returned by CookieStore.wrap()
       That is, it will be a string of the form
       "host:path:name". */

    /* TODO verify that colons can't normally appear in any of
       the fields -- if they did it then we can't rely on .split(":")
       to parse correctly.*/

    let cookieArray = GUID.split( ":" );
    let cookieHost = cookieArray[0];
    let cookiePath = cookieArray[1];
    let cookieName = cookieArray[2];

    /* alternate implementation would be to instantiate a cookie from
       cookieHost, cookiePath, and cookieName, then call
       cookieManager.cookieExists(). Maybe that would have better
       performance?  This implementation seems pretty slow.*/
    let enumerator = this._cookieManager.enumerator;
    while (enumerator.hasMoreElements())
      {
	let aCookie = enumerator.getNext();
	if (aCookie.host == cookieHost &&
	    aCookie.path == cookiePath &&
	    aCookie.name == cookieName ) {
	  return true;
	}
      }
    return false;
    /* Note: We can't just call cookieManager.cookieExists() with a generic
       javascript object with .host, .path, and .name attributes attatched.
       cookieExists is implemented in C and does a hard static_cast to an
       nsCookie object, so duck typing doesn't work (and in fact makes
       Firefox hard-crash as the static_cast returns null and is not checked.)
    */
  },

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

    this._log.info("CookieStore got createCommand: " + command );
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

    this._log.info("CookieStore got removeCommand: " + command );

    /* I think it goes like this, according to
      http://developer.mozilla.org/en/docs/nsICookieManager
      the last argument is "always block cookies from this domain?"
      and the answer is "no". */
    this._cookieManager.remove( command.data.host,
				command.data.name,
				command.data.path,
				false );
  },

  _editCommand: function CookieStore__editCommand(command) {
    /* we got a command to change a cookie in the local browser
       in order to sync with the server. */
    this._log.info("CookieStore got editCommand: " + command );

    /* Look up the cookie that matches the one in the command: */
    var iter = this._cookieManager.enumerator;
    var matchingCookie = null;
    while (iter.hasMoreElements()){
      let cookie = iter.getNext();
      if (cookie.QueryInterface( Ci.nsICookie ) ){
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
    while (iter.hasMoreElements()){
      var cookie = iter.getNext();
      if (cookie.QueryInterface( Ci.nsICookie )){
	// String used to identify cookies is
	// host:path:name
	if ( cookie.isSession ) {
	  /* Skip session-only cookies, sync only persistent cookies. */
	  continue;
	}

	let key = cookie.host + ":" + cookie.path + ":" + cookie.name;
	items[ key ] = { parentGUID: '',
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
    return items;
  },

  wipe: function CookieStore_wipe() {
    /* Remove everything from the store.  Return nothing.
       TODO are the semantics of this just wiping out an internal
       buffer, or am I supposed to wipe out all cookies from
       the browser itself for reals? */
    this._cookieManager.removeAll();
  },

  resetGUIDs: function CookieStore_resetGUIDs() {
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
