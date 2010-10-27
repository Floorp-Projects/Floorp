// ***** BEGIN LICENSE BLOCK *****
// Version: MPL 1.1/GPL 2.0/LGPL 2.1
// 
// The contents of this file are subject to the Mozilla Public License Version
// 1.1 (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
// http://www.mozilla.org/MPL/
// 
// Software distributed under the License is distributed on an "AS IS" basis,
// WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
// for the specific language governing rights and limitations under the
// License.
// 
// The Original Code is Mozilla Corporation Code.
// 
// The Initial Developer of the Original Code is
// based on the MozRepl project.
// Portions created by the Initial Developer are Copyright (C) 2008
// the Initial Developer. All Rights Reserved.
// 
// Contributor(s):
//  Mikeal Rogers <mikeal.rogers@gmail.com>
//  Massimiliano Mirra <bard@hyperstruct.net>
//
// Alternatively, the contents of this file may be used under the terms of
// either the GNU General Public License Version 2 or later (the "GPL"), or
// the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
// in which case the provisions of the GPL or the LGPL are applicable instead
// of those above. If you wish to allow use of your version of this file only
// under the terms of either the GPL or the LGPL, and not to allow others to
// use your version of this file under the terms of the MPL, indicate your
// decision by deleting the provisions above and replace them with the notice
// and other provisions required by the GPL or the LGPL. If you do not delete
// the provisions above, a recipient may use your version of this file under
// the terms of any one of the MPL, the GPL or the LGPL.
// 
// ***** END LICENSE BLOCK *****

var EXPORTED_SYMBOLS = ["Server", "server", "AsyncRead", "Session", "sessions", "globalRegistry", "startServer"];

var events = {}; Components.utils.import("resource://jsbridge/modules/events.js", events);

const Cc = Components.classes;
const Ci = Components.interfaces;
const loader = Cc['@mozilla.org/moz/jssubscript-loader;1']
    .getService(Ci.mozIJSSubScriptLoader);

var hwindow = Components.classes["@mozilla.org/appshell/appShellService;1"]
    .getService(Components.interfaces.nsIAppShellService)
    .hiddenDOMWindow;
    
var nativeJSON = Components.classes["@mozilla.org/dom/json;1"]
    .createInstance(Components.interfaces.nsIJSON);

var json2 = Components.utils.import("resource://jsbridge/modules/json2.js");

var jsonEncode = json2.JSON.stringify;    

var uuidgen = Components.classes["@mozilla.org/uuid-generator;1"]
    .getService(Components.interfaces.nsIUUIDGenerator);

function AsyncRead (session) {
  this.session = session;
}
AsyncRead.prototype.onStartRequest = function (request, context) {};
AsyncRead.prototype.onStopRequest = function (request, context, status) {
  this.session.onQuit();
}
AsyncRead.prototype.onDataAvailable = function (request, context, inputStream, offset, count) {
  var str = {};
  this.session.instream.readString(count, str);
  this.session.receive(str.value);
}



globalRegistry = {};

function Bridge (session) {
  this.session = session;
  this.registry = globalRegistry;
}
Bridge.prototype._register = function (_type) {
  this.bridgeType = _type;
  if (_type == "backchannel") {
    events.addBackChannel(this);
  }
}
Bridge.prototype.register = function (uuid, _type) {
  try {
    this._register(_type);
    var passed = true;
  } catch(e) {
    if (typeof(e) == "string") {
      var exception = e;
    } else {
      var exception = {'name':e.name, 'message':e.message};
    }
    this.session.encodeOut({'result':false, 'exception':exception, 'uuid':uuid});
  }
  if (passed != undefined) {
    this.session.encodeOut({"result":true, 'eventType':'register', 'uuid':uuid});
  }
  
}
Bridge.prototype._describe = function (obj) {
  var response = {};
  if (obj == null) {
    var type = "null";
  } else {
    var type = typeof(obj);
  }
  if (type == "object") {
    if (obj.length != undefined) {
      var type = "array";
    }
    response.attributes = [];
    for (i in obj) {
      response.attributes = response.attributes.concat(i);
    }
  }
  else if (type != "function"){
    response.data = obj;
  }
  response.type = type;
  return response;
}
Bridge.prototype.describe = function (uuid, obj) {
  var response = this._describe(obj);
  response.uuid = uuid;
  response.result = true;
  this.session.encodeOut(response);
}
Bridge.prototype._set = function (obj) {
  var uuid = uuidgen.generateUUID().toString();
  this.registry[uuid] = obj;
  return uuid;
}
Bridge.prototype.set = function (uuid, obj) {
  var ruuid = this._set(obj);
  this.session.encodeOut({'result':true, 'data':'bridge.registry["'+ruuid+'"]', 'uuid':uuid});
}
Bridge.prototype._setAttribute = function (obj, name, value) {
  obj[name] = value;
  return value;
}
Bridge.prototype.setAttribute = function (uuid, obj, name, value) {
  // log(uuid, String(obj), name, String(value))
  try {
    var result = this._setAttribute(obj, name, value);
  } catch(e) {
    if (typeof(e) == "string") {
      var exception = e;
    } else {
      var exception = {'name':e.name, 'message':e.message};
    }
    this.session.encodeOut({'result':false, 'exception':exception, 'uuid':uuid});
  }
  if (result != undefined) {
    this.set(uuid, obj[name]);
  }
}
Bridge.prototype._execFunction = function (func, args) {
  return func.apply(this.session.sandbox, args);
}
Bridge.prototype.execFunction = function (uuid, func, args) {
  try {
    var data = this._execFunction(func, args);
    var result = true;
  } catch(e) {
    if (typeof(e) == "string") {
      var exception = e;
    } else {
      var exception = {'name':e.name, 'message':e.message};
    }
    this.session.encodeOut({'result':false, 'exception':exception, 'uuid':uuid});
    var result = true;
  }  
  if (data != undefined) {
    this.set(uuid, data);
  } else if ( result == true) {
    this.session.encodeOut({'result':true, 'data':null, 'uuid':uuid});
  } else {
    throw 'Something very bad happened.'
  }
}

backstage = this;

function Session (transport) {
  this.transpart = transport;
  this.sandbox = Components.utils.Sandbox(backstage);
  this.sandbox.bridge = new Bridge(this);
  this.sandbox.openPreferences = hwindow.openPreferences;
  try {
      this.outputstream = transport.openOutputStream(Ci.nsITransport.OPEN_BLOCKING, 0, 0);	
      this.outstream = Cc['@mozilla.org/intl/converter-output-stream;1']
                    .createInstance(Ci.nsIConverterOutputStream);
      this.outstream.init(this.outputstream, 'UTF-8', 1024,
                    Ci.nsIConverterOutputStream.DEFAULT_REPLACEMENT_CHARACTER);
      this.stream = transport.openInputStream(0, 0, 0);
      this.instream = Cc['@mozilla.org/intl/converter-input-stream;1']
          .createInstance(Ci.nsIConverterInputStream);
      this.instream.init(this.stream, 'UTF-8', 1024,
                    Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);
  } catch(e) {
      log('jsbridge: Error: ' + e);
  }
  // log('jsbridge: Accepted connection.');
  
  this.pump = Cc['@mozilla.org/network/input-stream-pump;1']
      .createInstance(Ci.nsIInputStreamPump);
  this.pump.init(this.stream, -1, -1, 0, 0, false);
  this.pump.asyncRead(new AsyncRead(this), null);
}
Session.prototype.onOutput = function(string) {
  // log('jsbridge write: '+string)
  if (typeof(string) != "string") {
    throw "This is not a string"
  } 
  try {
    this.outstream.writeString(string);
    this.outstream.flush();
  } catch (e) {
    throw "Why is this failing "+string
  }
  // this.outstream.write(string, string.length);
};
Session.prototype.onQuit = function() {
  this.instream.close();
  this.outstream.close();
  sessions.remove(session);
};
Session.prototype.encodeOut = function (obj) {
  try {
    this.onOutput(jsonEncode(obj));
  } catch(e) {
    if (typeof(e) == "string") {
      var exception = e;
    } else {
      var exception = {'name':e.name, 'message':e.message};
    }
    this.onOutput(jsonEncode({'result':false, 'exception':exception}));
  }
  
}
Session.prototype.receive = function(data) {
  // log('jsbrige receive: '+data);
  Components.utils.evalInSandbox(data, this.sandbox);
}

var sessions = {
    _list: [],
    add: function(session) {
        this._list.push(session);
    },
    remove: function(session) {
        var index = this._list.indexOf(session);
        if(index != -1)
            this._list.splice(index, 1);
    },
    get: function(index) {
        return this._list[index];
    },
    quit: function() {
        this._list.forEach(
            function(session) { session.quit; });
        this._list.splice(0, this._list.length);
    }
};

function Server (port) {
  this.port = port;
}
Server.prototype.start = function () {
  try {
    this.serv = Cc['@mozilla.org/network/server-socket;1']
        .createInstance(Ci.nsIServerSocket);
    this.serv.init(this.port, true, -1);
    this.serv.asyncListen(this);
    // log('jsbridge: Listening...');
  } catch(e) {
    log('jsbridge: Exception: ' + e);
  }    
}
Server.prototype.stop = function () {
    log('jsbridge: Closing...');
    this.serv.close();
    this.sessions.quit();
    this.serv = undefined;
}
Server.prototype.onStopListening = function (serv, status) {
// Stub function
}
Server.prototype.onSocketAccepted = function (serv, transport) {
  session = new Session(transport)
  sessions.add(session);
}

function log(msg) {
    dump(msg + '\n');
}

function startServer(port) {
  var server = new Server(port)
  server.start()
}


