"use strict";

/* globals addMessageListener */
let {Management} = Components.utils.import("resource://gre/modules/Extension.jsm", {});
let events = [];
function record(type, extensionContext) {
  let eventType = type == "page-load" || type == "proxy-context-load" ? "load" : "unload";
  let url = extensionContext.uri.spec;
  let extensionId = extensionContext.extension.id;
  events.push({eventType, url, extensionId});
}

Management.on("page-load", record);
Management.on("page-unload", record);
Management.on("proxy-context-load", record);
Management.on("proxy-context-unload", record);
addMessageListener("cleanup", () => {
  Management.off("page-load", record);
  Management.off("page-unload", record);
  Management.off("proxy-context-load", record);
  Management.off("proxy-context-unload", record);
});

addMessageListener("get-context-events", extensionId => {
  sendAsyncMessage("context-events", events);
  events = [];
});
sendAsyncMessage("chromescript-startup");
