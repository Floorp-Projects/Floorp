/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals require, exports */

"use strict";

const { DevToolsServer } = require("devtools/server/devtools-server");
const { RootActor } = require("devtools/server/actors/root");
const { BrowserTabList } = require("devtools/server/actors/webbrowser");
const { ProcessActorList } = require("devtools/server/actors/process");
const {
  ActorRegistry,
} = require("devtools/server/actors/utils/actor-registry");

/**
 * background-task specific actors.
 *
 */

/**
 * Construct a root actor appropriate for use in a server running a background task.
 */
function createRootActor(connection) {
  let parameters = {
    tabList: new BackgroundTaskTabList(connection),
    processList: new ProcessActorList(),
    globalActorFactories: ActorRegistry.globalActorFactories,
    onShutdown() {},
  };
  return new RootActor(connection, parameters);
}
exports.createRootActor = createRootActor;

/**
 * A "stub" TabList implementation that provides no tabs.
 */
class BackgroundTaskTabList extends BrowserTabList {
  getList() {
    return Promise.resolve([]);
  }
}
