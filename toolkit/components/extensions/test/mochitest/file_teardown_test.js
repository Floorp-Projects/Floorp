"use strict";

/* globals addMessageListener */
let {InternalTestAPI} = Components.utils.import("resource://gre/modules/Extension.jsm");
let events = [];
function record(type, extensionContext) {
  let eventType = type == "page-load" ? "load" : "unload";
  let url = extensionContext.uri.spec;
  let {extensionId} = extensionContext;
  events.push({eventType, url, extensionId});
}

InternalTestAPI.on("page-load", record);
InternalTestAPI.on("page-unload", record);
addMessageListener("cleanup", () => {
  InternalTestAPI.off("page-load", record);
  InternalTestAPI.off("page-unload", record);
});

addMessageListener("get-context-events", extensionId => {
  sendAsyncMessage("context-events", events);
  events = [];
});
sendAsyncMessage("chromescript-startup");
