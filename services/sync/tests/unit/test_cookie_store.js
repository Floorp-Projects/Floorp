Components.utils.import("resource://weave/engines/cookies.js");

function FakeCookie( host, path, name, value,
                     isSecure, isHttpOnly, isSession, expiry ) {
  this._init( host, path, name, value,
              isSecure, isHttpOnly, isSession, expiry );
}
FakeCookie.prototype = {
  _init: function( host, path, name, value,
                   isSecure, isHttpOnly, isSession, expiry) {
    this.host = host;
    this.path = path;
    this.name = name;
    this.value = value;
    this.isSecure = isSecure;
    this.isHttpOnly = isHttpOnly;
    this.isSession = isSession;
  },

  QueryInterface: function( aIID ) {
    if ( !aIID.equals( Components.interfaces.nsICookie2 ) ) {
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
    return this;
  }
};

function StubEnumerator( list ) {
  this._init( list );
}
StubEnumerator.prototype = {
  _init: function( list ) {
    this._list = list;
    this._pointer = 0;
  },
  hasMoreElements: function() {
    return ( this._list.length > this._pointer );
  },
  getNext: function() {
    var theThing = this._list[ this._pointer ];
    this._pointer++;
    return theThing;
  }
};

function FakeCookieManager() {
  this._init();
}
FakeCookieManager.prototype = {
  _init: function() {
    this._cookieList = [];
  },

  add: function( host, path, name, value,
                 isSecure, isHttpOnly, isSession, expiry) {
    var newCookie = new FakeCookie( host,
                                    path,
                                    name,
                                    value,
                                    isSecure,
                                    isHttpOnly,
                                    isSession,
                                    expiry );
    this._cookieList.push( newCookie );
  },
  remove: function( host, name, path, alwaysBlock ) {
    for (var x in this._cookieList ) {
      var cookie = this._cookieList[x];
      if ( cookie.host == host &&
           cookie.name == name &&
           cookie.path == path ) {
          this._cookieList.splice( x, 1 );
          break;
      }
    }
  },

  get enumerator() {
    var stubEnum = new StubEnumerator( this._cookieList );
    return stubEnum;
  },

  removeAll: function() {
    this._cookieList = [];
  }
};

function sub_test_cookie_tracker() {
  var ct = new CookieTracker();

  // gonna have to use the real cookie manager here...
  var cookieManager = Cc["@mozilla.org/cookiemanager;1"].
                             getService(Ci.nsICookieManager2);
  var d = new Date();
  d.setDate( d.getDate() + 1 );
  cookieManager.add( "www.evilbrainjono.net",
                     "/blog/",
                     "comments",
                     "on",
                     false,
                     true,
                     false,
                     d.getTime() );
  cookieManager.add( "www.evilbrainjono.net",
                     "/comic/",
                     "lang",
                     "jp",
                     false,
                     true,
                     false,
                     d.getTime() );
  cookieManager.add( "www.evilbrainjono.net",
                     "/blog/",
                     "comments",
                     "off",
                     false,
                     true,
                     true, // session
                     d.getTime() );
  // score is 10 per cookie changed, but we should be ignoring the
  // session cookie, so we should be at 20 now.
  do_check_eq( ct.score, 20 );
};

function run_test() {
  /* Set a persistent cookie and a non-persistent cookie
  then call cookieStore.wrap() and make sure it returns the persistent
  one and not the non-persistent one */

  // My stub object to replace the real cookieManager:
  var fakeCookieManager = new FakeCookieManager();

  // add a persistent cookie:
  var d = new Date();
  d.setDate( d.getDate() + 1 );
  fakeCookieManager.add( "evilbrainjono.net", "/", "login", "jono",
			 false, true, false, d.getTime() );
  // and a session cookie:
  fakeCookieManager.add( "humanized.com", "/", "langauge", "en",
			 false, true, true, 0 );
  var myStore = new CookieStore( fakeCookieManager );
  var json = myStore.wrap();
  // The json should include only the persistent cookie, not the session
  // cookie:

  var jsonGuids = [ guid for ( guid in json ) ];
  do_check_eq( jsonGuids.length, 1 );
  do_check_eq( jsonGuids[0], "evilbrainjono.net:/:login" );
  sub_test_cookie_tracker();
}
