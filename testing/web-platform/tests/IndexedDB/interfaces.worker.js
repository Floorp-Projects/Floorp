"use strict";

importScripts("/resources/testharness.js");
importScripts("/resources/WebIDLParser.js", "/resources/idlharness.js");

var request = new XMLHttpRequest();
request.open("GET", "interfaces.idl");
request.send();
request.onload = function() {
  var idlArray = new IdlArray();
  var idls = request.responseText;

  idlArray.add_untested_idls("[Exposed=Worker] interface WorkerGlobalScope {};");
  idlArray.add_untested_idls("[Exposed=(Window,Worker)] interface Event { };");
  idlArray.add_untested_idls("[Exposed=(Window,Worker)] interface EventTarget { };");
  idlArray.add_untested_idls("[NoInterfaceObject, Exposed=(Window,Worker)] interface WindowOrWorkerGlobalScope {};");

  // From Indexed DB:
  idlArray.add_idls("WorkerGlobalScope implements WindowOrWorkerGlobalScope;");
  idlArray.add_idls(idls);

  idlArray.add_objects({
    IDBCursor: [],
    IDBCursorWithValue: [],
    IDBDatabase: [],
    IDBFactory: ["self.indexedDB"],
    IDBIndex: [],
    IDBKeyRange: ["IDBKeyRange.only(0)"],
    IDBObjectStore: [],
    IDBOpenDBRequest: [],
    IDBRequest: [],
    IDBTransaction: [],
    IDBVersionChangeEvent: ["new IDBVersionChangeEvent('foo')"],
  });
  idlArray.test();
  done();
};
