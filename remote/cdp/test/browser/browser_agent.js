/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// To fully test the Remote Agent's capabilities an instance of the interface
// also needs to be used.
const remoteAgentInstance = Cc["@mozilla.org/remote/agent;1"].createInstance(
  Ci.nsIRemoteAgent
);

add_task(async function running() {
  is(remoteAgentInstance.running, true, "Agent is running");
});
