/* vim:set ts=2 sw=2 et: */
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

/*
 * To use try out this JS server socket implementation, just copy this file
 * into the "components" directory of a Mozilla build.  Then load the URL
 * http://localhost:4444/ in the browser.  You should see a page get loaded
 * that was served up by this component :-)
 *
 * This code requires Mozilla 1.6 or better.
 */

const kTESTSERV_CONTRACTID = "@mozilla.org/network/test-serv;1";
const kTESTSERV_CID = Components.ID("{a741fcd5-9695-42e8-a7f7-14f9a29f8200}");
const nsISupports = Components.interfaces.nsISupports;
const nsIObserver = Components.interfaces.nsIObserver;
const nsIServerSocket = Components.interfaces.nsIServerSocket;
const nsIServerSocketListener = Components.interfaces.nsIServerSocketListener;
const nsITransport = Components.interfaces.nsITransport;
const nsIScriptableInputStream = Components.interfaces.nsIScriptableInputStream;

/** we'll listen on this port for HTTP requests **/
const kPORT = 4444;

function nsTestServ() { dump(">>> creating nsTestServ instance\n"); };

nsTestServ.prototype =
{
  QueryInterface: function(iid)
  {
    if (iid.equals(nsIObserver) ||
        iid.equals(nsIServerSocketListener) ||
        iid.equals(nsISupports))
      return this;

    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
  },

  observe: function(subject, topic, data)
  {
    dump(">>> observe [" + topic + "]\n");
    this.startListening();
  },

  /* this function is called when we receive a new connection */
  onSocketAccepted: function(serverSocket, clientSocket)
  {
    dump(">>> accepted connection on "+clientSocket.host+":"+clientSocket.port+"\n");

    var input = clientSocket.openInputStream(nsITransport.OPEN_BLOCKING, 0, 0);
    var output = clientSocket.openOutputStream(nsITransport.OPEN_BLOCKING, 0, 0);

    this.consumeInput(input);

    const fixedResponse =
      "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\nFooooopy!!\r\n";
    var response = fixedResponse + "\r\n" + new Date().toString() + "\r\n";
    var n = output.write(response, response.length);
    dump(">>> wrote "+n+" bytes\n");

    input.close();
    output.close();
  },

  onStopListening: function(serverSocket, status)
  {
    dump(">>> shutting down server socket\n");
  },

  startListening: function()
  {
    const SERVERSOCKET_CONTRACTID = "@mozilla.org/network/server-socket;1";
    var socket = Components.classes[SERVERSOCKET_CONTRACTID].createInstance(nsIServerSocket);
    socket.init(kPORT, true /* loopback only */, 5);
    dump(">>> listening on port "+socket.port+"\n");
    socket.asyncListen(this);
  },

  consumeInput: function(input)
  {
    /* use nsIScriptableInputStream to consume all of the data on the stream */

    var sin = Components.classes["@mozilla.org/scriptableinputstream;1"]
                        .createInstance(nsIScriptableInputStream);
    sin.init(input);

    /* discard all data */
    while (sin.available() > 0)
      sin.read(512);
  }
}

/**
 * JS XPCOM component registration goop:
 *
 * We set ourselves up to observe the xpcom-startup category.  This provides
 * us with a starting point.
 */

var servModule = new Object();

servModule.registerSelf =
function (compMgr, fileSpec, location, type)
{
  compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
  compMgr.registerFactoryLocation(kTESTSERV_CID,
                                  "nsTestServ",
                                  kTESTSERV_CONTRACTID,
                                  fileSpec,
                                  location,
                                  type);

  const CATMAN_CONTRACTID = "@mozilla.org/categorymanager;1";
  const nsICategoryManager = Components.interfaces.nsICategoryManager;
  var catman = Components.classes[CATMAN_CONTRACTID].getService(nsICategoryManager);
  catman.addCategoryEntry("xpcom-startup",
                          "TestServ",
                          kTESTSERV_CONTRACTID,
                          true,
                          true);
}

servModule.getClassObject =
function (compMgr, cid, iid)
{
  if (!cid.equals(kTESTSERV_CID))
    throw Components.results.NS_ERROR_NO_INTERFACE;

  if (!iid.equals(Components.interfaces.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  return servFactory;
}

servModule.canUnload =
function (compMgr)
{
  dump(">>> unloading test serv.\n");
  return true;
}

var servFactory = new Object();

servFactory.createInstance =
function (outer, iid)
{
  if (outer != null)
    throw Components.results.NS_ERROR_NO_AGGREGATION;

  if (!iid.equals(nsIObserver) &&
      !iid.equals(nsISupports))
    throw Components.results.NS_ERROR_NO_INTERFACE;

  return TestServ;
}

function NSGetModule(compMgr, fileSpec)
{
  return servModule;
}

var TestServ = new nsTestServ();
