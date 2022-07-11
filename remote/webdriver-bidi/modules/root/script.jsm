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
   * Calls a provided function with given arguments and scope in the provided
   * target, which is either a realm or a browsing context.
   *
   * @param {Object=} options
   * @param {Array<RemoteValue>=} arguments
   *     The arguments to pass to the function call. [unsupported]
   * @param {boolean} awaitPromise
   *     Determines if the command should wait for the return value of the
   *     expression to resolve, if this return value is a Promise.
   * @param {string} functionDeclaration
   *     The expression to evaluate.
   * @param {OwnershipModel=} resultOwnership [unsupported]
   *     The ownership model to use for the results of this evaluation.
   * @param {Object} target
   *     The target for the evaluation, which either matches the definition for
   *     a RealmTarget or for ContextTarget.
   * @param {RemoteValue=} this
   *     The value of the this keyword for the function call. [unsupported]
   *
   * @returns {ScriptEvaluateResult}
   *
   * @throws {InvalidArgumentError}
   *     If any of the arguments has not the expected type.
   * @throws {NoSuchFrameError}
   *     If the target cannot be found.
   */
  async callFunction(options = {}) {
    const {
      arguments: commandArguments = null,
      awaitPromise,
      functionDeclaration,
      resultOwnership = OwnershipModel.None,
      target = {},
      this: thisParameter = null,
    } = options;

    lazy.assert.string(
      functionDeclaration,
      `Expected "functionDeclaration" to be a string, got ${functionDeclaration}`
    );

    lazy.assert.boolean(
      awaitPromise,
      `Expected "awaitPromise" to be a boolean, got ${awaitPromise}`
    );

    this.#assertResultOwnership(resultOwnership);

    if (commandArguments != null) {
      lazy.assert.array(
        commandArguments,
        `Expected "arguments" to be an array, got ${commandArguments}`
      );
      throw new lazy.error.UnsupportedOperationError(
        `"arguments" parameter is not supported yet`
      );
    }

    if (thisParameter != null) {
      throw new lazy.error.UnsupportedOperationError(
        `"this" parameter is not supported yet`
      );
    }

    const { contextId, realmId, sandbox } = this.#assertTarget(target);
    const realm = this.#getRealmInfoFromTarget({ contextId, realmId, sandbox });
    const evaluationResult = await this.messageHandler.forwardCommand({
      moduleName: "script",
      commandName: "callFunctionDeclaration",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: realm.context.id,
      },
      params: {
        awaitPromise,
        commandArguments,
        functionDeclaration,
      },
    });

    return this.#buildReturnValue(evaluationResult, realm);
  }

  /**
   * Evaluate a provided expression in the provided target, which is either a
   * realm or a browsing context.
   *
   * @param {Object=} options
   * @param {boolean} awaitPromise
   *     Determines if the command should wait for the return value of the
   *     expression to resolve, if this return value is a Promise.
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
      awaitPromise,
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

    this.#assertResultOwnership(resultOwnership);

    const { contextId, realmId, sandbox } = this.#assertTarget(target);
    const realm = this.#getRealmInfoFromTarget({ contextId, realmId, sandbox });
    const evaluationResult = await this.messageHandler.forwardCommand({
      moduleName: "script",
      commandName: "evaluateExpression",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: realm.context.id,
      },
      params: {
        awaitPromise,
        expression: source,
      },
    });

    return this.#buildReturnValue(evaluationResult, realm);
  }

  #assertResultOwnership(resultOwnership) {
    if (![OwnershipModel.None, OwnershipModel.Root].includes(resultOwnership)) {
      throw new lazy.error.InvalidArgumentError(
        `Expected "resultOwnership" to be one of ${Object.values(
          OwnershipModel
        )}, got ${resultOwnership}`
      );
    }
  }

  #assertTarget(target) {
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

    return { contextId, realmId, sandbox };
  }

  #buildReturnValue(evaluationResult, realm) {
    const rv = { realm: realm.realm };
    switch (evaluationResult.evaluationStatus) {
      // TODO: Compare with EvaluationStatus.Normal after Bug 1774444 is fixed.
      case "normal":
        rv.result = evaluationResult.result;
        break;
      // TODO: Compare with EvaluationStatus.Throw after Bug 1774444 is fixed.
      case "throw":
        rv.exceptionDetails = evaluationResult.exceptionDetails;
        break;
      default:
        throw new lazy.error.UnsupportedOperationError(
          `Unsupported evaluation status ${evaluationResult.evaluationStatus}`
        );
    }
    return rv;
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

      if (!context.currentWindowGlobal) {
        throw new lazy.error.NoSuchFrameError(
          `No window found for BrowsingContext with id ${contextId}`
        );
      }

      // TODO: Return an actual realm once we have proper realm support.
      // See Bug 1766240.
      return {
        realm: String(context.currentWindowGlobal.innerWindowId),
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
