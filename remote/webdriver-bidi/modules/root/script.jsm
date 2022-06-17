/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["script"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.jsm",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  TabManager: "chrome://remote/content/shared/TabManager.jsm",
  WindowGlobalMessageHandler:
    "chrome://remote/content/shared/messagehandler/WindowGlobalMessageHandler.jsm",
});

/**
 * @typedef {Object} OwnershipModel
 **/

/**
 * Enum of ownership models supported by the script module.
 *
 * @readonly
 * @enum {OwnershipModel}
 **/
const OwnershipModel = {
  None: "none",
  Root: "root",
};

/**
 * @typedef {Object} RealmType
 **/

/**
 * Enum of realm types.
 *
 * @readonly
 * @enum {RealmType}
 **/
const RealmType = {
  AudioWorklet: "audio-worklet",
  DedicatedWorker: "dedicated-worker",
  PaintWorklet: "paint-worklet",
  ServiceWorker: "service-worker",
  SharedWorker: "shared-worker",
  Window: "window",
  Worker: "worker",
  Worklet: "worklet",
};

class ScriptModule extends Module {
  destroy() {}

  /**
   * Used to represent a frame of a JavaScript stack trace.
   *
   * @typedef StackFrame
   *
   * @property {number} columnNumber
   * @property {string} functionName
   * @property {number} lineNumber
   * @property {string} url
   */

  /**
   * Used to represent a JavaScript stack at a point in script execution.
   *
   * @typedef StackTrace
   *
   * @property {Array<StackFrame>} callFrames
   */

  /**
   * Used to represent a JavaScript exception.
   *
   * @typedef ExceptionDetails
   *
   * @property {number} columnNumber
   * @property {RemoteValue} exception
   * @property {number} lineNumber
   * @property {StackTrace} stackTrace
   * @property {string} text
   */

  /**
   * Used as return value for script.evaluate, as one of the available variants
   * {ScriptEvaluateResultException} or {ScriptEvaluateResultSuccess}.
   *
   * @typedef ScriptEvaluateResult
   */

  /**
   * Used as return value for script.evaluate when the script completes with a
   * thrown exception.
   *
   * @typedef ScriptEvaluateResultException
   *
   * @property {ExceptionDetails} exceptionDetails
   * @property {string} realm
   */

  /**
   * Used as return value for script.evaluate when the script completes
   * normally.
   *
   * @typedef ScriptEvaluateResultSuccess
   *
   * @property {string} realm
   * @property {RemoteValue} result
   */

  /**
   * Evaluate a provided expression in the provided target, which is either a
   * realm or a browsing context.
   *
   * @param {Object=} options
   * @param {boolean=} awaitPromise [unsupported]
   *     Determines if the command should wait for the return value of the
   *     expression to resolve, if this return value is a Promise. Defaults to
   *     true.
   * @param {string} expression
   *     The expression to evaluate.
   * @param {OwnershipModel=} resultOwnership [unsupported]
   *     The ownership model to use for the results of this evaluation.
   * @param {Object} target
   *     The target for the evaluation, which either matches the definition for
   *     a RealmTarget or for ContextTarget.
   *
   * @returns {ScriptEvaluateResult}
   *
   * @throws {InvalidArgumentError}
   *     If any of the arguments has not the expected type.
   * @throws {NoSuchFrameError}
   *     If the target cannot be found.
   */
  async evaluate(options = {}) {
    const {
      awaitPromise = true,
      expression: source,
      resultOwnership = OwnershipModel.None,
      target = {},
    } = options;

    lazy.assert.string(
      source,
      `Expected "expression" to be a string, got ${source}`
    );

    lazy.assert.boolean(
      awaitPromise,
      `Expected "awaitPromise" to be a boolean, got ${awaitPromise}`
    );

    if (![OwnershipModel.None, OwnershipModel.Root].includes(resultOwnership)) {
      throw new lazy.error.InvalidArgumentError(
        `Expected "resultOwnership" to be one of ${Object.values(
          OwnershipModel
        )}, got ${resultOwnership}`
      );
    }

    lazy.assert.object(
      target,
      `Expected "target" to be an object, got ${target}`
    );

    const {
      context: contextId = null,
      realm: realmId = null,
      sandbox = null,
    } = target;
    if (contextId != null) {
      lazy.assert.string(
        contextId,
        `Expected "context" to be a string, got ${contextId}`
      );

      if (sandbox != null) {
        lazy.assert.string(
          sandbox,
          `Expected "sandbox" to be a string, got ${sandbox}`
        );
        throw new lazy.error.UnsupportedOperationError(
          `sandbox is not supported yet`
        );
      }
    } else if (realmId != null) {
      lazy.assert.string(
        realmId,
        `Expected "realm" to be a string, got ${realmId}`
      );
      throw new lazy.error.UnsupportedOperationError(
        `realm is not supported yet`
      );
    } else {
      throw new lazy.error.InvalidArgumentError(`No context or realm provided`);
    }

    const realm = this.#getRealmInfoFromTarget({ contextId, realmId, sandbox });
    const { result } = await this.messageHandler.forwardCommand({
      moduleName: "script",
      commandName: "evaluateExpression",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: realm.context.id,
      },
      params: {
        expression: source,
      },
    });

    return {
      result,
      realm: realm.realm,
    };
  }

  #getRealmInfoFromTarget({ contextId, realmId, sandbox }) {
    // Only supports WindowRealmInfo at the moment.
    if (contextId != null) {
      const context = lazy.TabManager.getBrowsingContextById(contextId);
      if (context === null) {
        throw new lazy.error.NoSuchFrameError(
          `Browsing Context with id ${contextId} not found`
        );
      }

      // TODO: Return an actual realm once we have proper realm support.
      // See Bug 1766240.
      return {
        realm: null,
        origin: null,
        type: RealmType.Window,
        context,
      };
    }

    throw new lazy.error.NoSuchFrameError(
      `No realm matching context: ${contextId}, realm: ${realmId}, sandbox: ${sandbox}`
    );
  }

  static get supportedEvents() {
    return [];
  }
}

const script = ScriptModule;
