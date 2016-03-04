/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const logger = Log.repository.getLogger("Marionette");

this.EXPORTED_SYMBOLS = ["evaluate"];

this.evaluate = {};

/**
 * Stores scripts imported from the local end through the
 * {@code GeckoDriver#importScript} command.
 *
 * Imported scripts are prepended to the script that is evaluated
 * on each call to {@code GeckoDriver#executeScript},
 * {@code GeckoDriver#executeAsyncScript}, and
 * {@code GeckoDriver#executeJSScript}.
 *
 * Usage:
 *
 *     let importedScripts = new evaluate.ScriptStorage();
 *     importedScripts.add(firstScript);
 *     importedScripts.add(secondScript);
 *
 *     let scriptToEval = importedScripts.concat(script);
 *     // firstScript and secondScript are prepended to script
 *
 */
evaluate.ScriptStorage = class extends Set {

  /**
   * Produce a string of all stored scripts.
   *
   * The stored scripts are concatenated into a string, with optional
   * additional scripts then appended.
   *
   * @param {...string} addional
   *     Optional scripts to include.
   *
   * @return {string}
   *     Concatenated string consisting of stored scripts and additional
   *     scripts, in that order.
   */
  concat(...additional) {
    let rv = "";
    for (let s of this) {
      rv = s + rv;
    }
    for (let s of additional) {
      rv = rv + s;
    }
    return rv;
  }

  toJson() {
    return Array.from(this);
  }

};

/**
 * Service that enables the script storage service to be queried from
 * content space.
 *
 * The storage can back multiple |ScriptStorage|, each typically belonging
 * to a |Context|.  Since imported scripts' scope are global and not
 * scoped to the current browsing context, all imported scripts are stored
 * in chrome space and fetched by content space as needed.
 *
 * Usage in chrome space:
 *
 *     let service = new evaluate.ScriptStorageService(
 *         [Context.CHROME, Context.CONTENT]);
 *     let storage = service.for(Context.CHROME);
 *     let scriptToEval = storage.concat(script);
 *
 */
evaluate.ScriptStorageService = class extends Map {

  /**
   * Create the service.
   *
   * An optional array of names for script storages to initially create
   * can be provided.
   *
   * @param {Array.<string>=} initialStorages
   *     List of names of the script storages to create initially.
   */
  constructor(initialStorages = []) {
    super(initialStorages.map(name => [name, new evaluate.ScriptStorage()]));
  }

  /**
   * Retrieve the scripts associated with the given context.
   *
   * @param {Context} context
   *     Context to retrieve the scripts from.
   *
   * @return {ScriptStorage}
   *     Scrips associated with given |context|.
   */
  for(context) {
    return this.get(context);
  }

  processMessage(msg) {
    switch (msg.name) {
      case "Marionette:getImportedScripts":
        let storage = this.for.apply(this, msg.json);
        return storage.toJson();

      default:
        throw new TypeError("Unknown message: " + msg.name);
    }
  }

  // TODO(ato): The idea of services in chrome space
  // can be generalised at some later time (see cookies.js:38).
  receiveMessage(msg) {
    try {
      return this.processMessage(msg);
    } catch (e) {
      logger.error(e);
    }
  }

};

evaluate.ScriptStorageService.prototype.QueryInterface =
    XPCOMUtils.generateQI([
      Ci.nsIMessageListener,
      Ci.nsISupportsWeakReference,
    ]);

/**
 * Bridges the script storage in chrome space, to make it possible to
 * retrieve a {@code ScriptStorage} associated with a given
 * {@code Context} from content space.
 *
 * Usage in content space:
 *
 *     let client = new evaluate.ScriptStorageServiceClient(chromeProxy);
 *     let storage = client.for(Context.CONTENT);
 *     let scriptToEval = storage.concat(script);
 *
 */
evaluate.ScriptStorageServiceClient = class {

  /**
   * @param {proxy.SyncChromeSender} chromeProxy
   *     Proxy for communicating with chrome space.
   */
  constructor(chromeProxy) {
    this.chrome = chromeProxy;
  }

  /**
   * Retrieve scripts associated with the given context.
   *
   * @param {Context} context
   *     Context to retrieve scripts from.
   *
   * @return {ScriptStorage}
   *     Scripts associated with given |context|.
   */
  for(context) {
    let scripts = this.chrome.getImportedScripts(context)[0];
    return new evaluate.ScriptStorage(scripts);
  }

};
