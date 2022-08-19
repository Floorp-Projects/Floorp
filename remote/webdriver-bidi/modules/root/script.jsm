/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["script"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
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
   *     The arguments to pass to the function call.
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
   *     The value of the this keyword for the function call.
   *
   * @returns {ScriptEvaluateResult}
   *
   * @throws {InvalidArgumentError}
   *     If any of the arguments has not the expected type.
   * @throws {NoSuchFrameError}
   *     If the target cannot be found.
   */
  async callFunction(options = {}) {
    // TODO: Bug 1778976. Remove once command is fully supported.
    this.assertExperimentalCommandsEnabled("script.callFunction");

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
    }

    const { contextId, realmId, sandbox } = this.#assertTarget(target);
    const context = this.#getContextFromTarget({ contextId, realmId, sandbox });
    const evaluationResult = await this.messageHandler.forwardCommand({
      moduleName: "script",
      commandName: "callFunctionDeclaration",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: context.id,
      },
      params: {
        awaitPromise,
        commandArguments,
        functionDeclaration,
        realmId,
        sandbox,
        thisParameter,
      },
    });

    return this.#buildReturnValue(evaluationResult);
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
    // TODO: Bug 1778976. Remove once command is fully supported.
    this.assertExperimentalCommandsEnabled("script.evaluate");

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
    const context = this.#getContextFromTarget({ contextId, realmId, sandbox });
    const evaluationResult = await this.messageHandler.forwardCommand({
      moduleName: "script",
      commandName: "evaluateExpression",
      destination: {
        type: lazy.WindowGlobalMessageHandler.type,
        id: context.id,
      },
      params: {
        awaitPromise,
        expression: source,
        realmId,
        sandbox,
      },
    });

    return this.#buildReturnValue(evaluationResult);
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

  #buildReturnValue(evaluationResult) {
    const rv = { realm: evaluationResult.realmId };
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

  // realmId is going to be used when the full Realm support is implemented
  // See Bug 1779231.
  #getContextFromTarget({ contextId /*, realmId, sandbox*/ }) {
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

    return context;
  }

  static get supportedEvents() {
    return [];
  }
}

const script = ScriptModule;
