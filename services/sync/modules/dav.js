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

const EXPORTED_SYMBOLS = ['DAVCollection'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/util.js");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Function.prototype.async = generatorAsync;

/*
 * DAV object
 * Abstracts the raw DAV commands
 */

function DAVCollection(baseURL) {
  this._baseURL = baseURL;
  this._authProvider = new DummyAuthProvider();
  this._log = Log4Moz.Service.getLogger("Service.DAV");
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
