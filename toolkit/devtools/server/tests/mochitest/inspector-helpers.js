var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

// Always log packets when running tests.
Services.prefs.setBoolPref("devtools.debugger.log", true);
SimpleTest.registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.debugger.log");
});

Cu.import("resource://gre/modules/devtools/Loader.jsm");
Cu.import("resource://gre/modules/devtools/dbg-client.jsm");
Cu.import("resource://gre/modules/devtools/dbg-server.jsm");

const {_documentWalker} = devtools.require("devtools/server/actors/inspector");

if (!DebuggerServer.initialized) {
  DebuggerServer.init(() => true);
  DebuggerServer.addBrowserActors();
  SimpleTest.registerCleanupFunction(function() {
    DebuggerServer.destroy();
  });
}

/**
 * Open a tab, load the url, wait for it to signal its readiness,
 * find the tab with the debugger server, and call the callback.
 */
function attachURL(url, callback) {
  var win = window.open(url, "_blank");

  SimpleTest.registerCleanupFunction(function() {
    win.close();
  });

  window.addEventListener("message", function loadListener(event) {
    if (event.data === "ready") {
      let client = new DebuggerClient(DebuggerServer.connectPipe());
      SimpleTest.registerCleanupFunction(function() {
        client.close();
      });

      client.connect((applicationType, traits) => {
        client.listTabs(response => {
          for (let tab of response.tabs) {
            if (tab.url === url) {
              window.removeEventListener("message", loadListener, false);
              try {
                callback(null, client, tab, win.document);
              } catch(ex) {
                Cu.reportError(ex);
                dump(ex);
              }
              break;
            }
          }
        });
      });
    }
  }, false);
}

function sortOwnershipChildren(children) {
  return children.sort((a, b) => a.name.localeCompare(b.name));
}

function serverOwnershipSubtree(walker, node) {
  let actor = walker._refMap.get(node);
  if (!actor) {
    return undefined;
  }

  let children = [];
  let docwalker = _documentWalker(node);
  let child = docwalker.firstChild();
  while (child) {
    let item = serverOwnershipSubtree(walker, child);
    if (item) {
      children.push(item);
    }
    child = docwalker.nextSibling();
  }
  return {
    name: actor.actorID,
    children: sortOwnershipChildren(children)
  }
}

function serverOwnershipTree(walker) {
  let serverConnection = walker.conn._transport._serverConnection;
  let serverWalker = serverConnection.getActor(walker.actorID);

  return serverOwnershipSubtree(serverWalker, serverWalker.rootDoc)
}

function clientOwnershipSubtree(node) {
  return {
    name: node.actorID,
    children: sortOwnershipChildren([clientOwnershipSubtree(child) for (child of node.treeChildren())])
  }
}

function clientOwnershipTree(walker) {
  return clientOwnershipSubtree(walker.rootNode);
}

function ownershipTreeSize(tree) {
  let size = 1;
  for (let child of tree.children) {
    size += ownershipTreeSize(child);
  }
  return size;
}

function assertOwnershipTrees(walker) {
  let serverTree = serverOwnershipTree(walker);
  let clientTree = clientOwnershipTree(walker);
  is(JSON.stringify(clientTree, null, ' '), JSON.stringify(serverTree, null, ' '), "Server and client ownership trees should match.");

  return ownershipTreeSize(clientTree);
}

// Verify that an actorID is inaccessible both from the client library and the server.
function checkMissing(client, actorID) {
  let deferred = Promise.defer();
  let front = client.getActor(actorID);
  ok(!front, "Front shouldn't be accessible from the client for actorID: " + actorID);

  let deferred = Promise.defer();
  client.request({
    to: actorID,
    type: "request",
  }, response => {
    is(response.error, "noSuchActor", "node list actor should no longer be contactable.");
    deferred.resolve(undefined);
  });
  return deferred.promise;
}

function promiseDone(promise) {
  promise.then(null, err => {
    ok(false, "Promise failed: " + err);
    if (err.stack) {
      dump(err.stack);
    }
    SimpleTest.finish();
  });
}

var _tests = [];
function addTest(test) {
  _tests.push(test);
}

function runNextTest() {
  if (_tests.length == 0) {
    SimpleTest.finish()
    return;
  }
  _tests.shift()();
}