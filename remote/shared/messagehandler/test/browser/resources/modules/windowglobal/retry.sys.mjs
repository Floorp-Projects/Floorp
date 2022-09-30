/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

// Store counters in the JSM scope to persist them across reloads.
let callsToBlockedOneTime = 0;
let callsToBlockedTenTimes = 0;
let callsToBlockedElevenTimes = 0;

// This module provides various commands which all hang for various reasons.
// The test is supposed to trigger the command and then destroy the
// JSWindowActor pair by any mean (eg a navigation) in order to trigger an
// AbortError and a retry.
class RetryModule extends Module {
  destroy() {}

  /**
   * Commands
   */

  // Resolves only if called while on the example.net domain.
  async blockedOnNetDomain(params) {
    // Note: we do not store a call counter here, because this is used for a
    // cross-group navigation test, and the JSM will be loaded in different
    // processes.
    const uri = this.messageHandler.window.document.baseURI;
    if (!uri.includes("example.net")) {
      await new Promise(r => {});
    }

    return { ...params };
  }

  // Resolves only if called more than once.
  async blockedOneTime(params) {
    callsToBlockedOneTime++;
    if (callsToBlockedOneTime < 2) {
      await new Promise(r => {});
    }

    // Return:
    // - params sent to the command to check that retries have correct params
    // - the call counter
    return { ...params, callsToCommand: callsToBlockedOneTime };
  }

  // Resolves only if called more than ten times (which is exactly the maximum
  // of retry attempts).
  async blockedTenTimes(params) {
    callsToBlockedTenTimes++;
    if (callsToBlockedTenTimes < 11) {
      await new Promise(r => {});
    }

    // Return:
    // - params sent to the command to check that retries have correct params
    // - the call counter
    return { ...params, callsToCommand: callsToBlockedTenTimes };
  }

  // Resolves only if called more than eleven times (which is greater than the
  // maximum of retry attempts).
  async blockedElevenTimes(params) {
    callsToBlockedElevenTimes++;
    if (callsToBlockedElevenTimes < 12) {
      await new Promise(r => {});
    }

    // Return:
    // - params sent to the command to check that retries have correct params
    // - the call counter
    return { ...params, callsToCommand: callsToBlockedElevenTimes };
  }

  cleanup() {
    callsToBlockedOneTime = 0;
    callsToBlockedTenTimes = 0;
    callsToBlockedElevenTimes = 0;
  }
}

export const retry = RetryModule;
