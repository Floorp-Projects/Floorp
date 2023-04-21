/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { WindowGlobalBiDiModule } from "chrome://remote/content/webdriver-bidi/modules/WindowGlobalBiDiModule.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  deserialize: "chrome://remote/content/webdriver-bidi/RemoteValue.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  getFramesFromStack: "chrome://remote/content/shared/Stack.sys.mjs",
  isChromeFrame: "chrome://remote/content/shared/Stack.sys.mjs",
  OwnershipModel: "chrome://remote/content/webdriver-bidi/RemoteValue.sys.mjs",
  serialize: "chrome://remote/content/webdriver-bidi/RemoteValue.sys.mjs",
  setDefaultSerializationOptions:
    "chrome://remote/content/webdriver-bidi/RemoteValue.sys.mjs",
  stringify: "chrome://remote/content/webdriver-bidi/RemoteValue.sys.mjs",
  WindowRealm: "chrome://remote/content/webdriver-bidi/Realm.sys.mjs",
});

/**
 * @typedef {string} EvaluationStatus
 */

/**
 * Enum of possible evaluation states.
 *
 * @readonly
 * @enum {EvaluationStatus}
 */
const EvaluationStatus = {
  Normal: "normal",
  Throw: "throw",
};

class ScriptModule extends WindowGlobalBiDiModule {
  #defaultRealm;
  #observerListening;
  #preloadScripts;
  #realms;

  constructor(messageHandler) {
    super(messageHandler);

    this.#defaultRealm = new lazy.WindowRealm(this.messageHandler.window);

    // Maps sandbox names to instances of window realms.
    this.#realms = new Map();

    // Set of structs with an item named expression, which is a string,
    // and an item named sandbox which is a string or null.
    this.#preloadScripts = new Set();
  }

  destroy() {
    this.#preloadScripts = null;

    this.#stopObserving();

    this.#defaultRealm.destroy();

    for (const realm of this.#realms.values()) {
      realm.destroy();
    }
    this.#realms = null;
  }

  observe(subject, topic) {
    if (topic !== "document-element-inserted") {
      return;
    }

    const window = subject?.defaultView;

    // Ignore events without a window and those from other tabs.
    if (window === this.messageHandler.window) {
      this.#evaluatePreloadScripts();
    }
  }

  #buildExceptionDetails(exception, stack, realm, resultOwnership, options) {
    exception = this.#toRawObject(exception);
    const frames = lazy.getFramesFromStack(stack) || [];

    const callFrames = frames
      // Remove chrome/internal frames
      .filter(frame => !lazy.isChromeFrame(frame))
      // Translate frames from getFramesFromStack to frames expected by
      // WebDriver BiDi.
      .map(frame => {
        return {
          columnNumber: frame.columnNumber - 1,
          functionName: frame.functionName,
          lineNumber: frame.lineNumber - 1,
          url: frame.filename,
        };
      });

    return {
      columnNumber: stack.column - 1,
      exception: lazy.serialize(
        exception,
        lazy.setDefaultSerializationOptions(),
        resultOwnership,
        new Map(),
        realm,
        options
      ),
      lineNumber: stack.line - 1,
      stackTrace: { callFrames },
      text: lazy.stringify(exception),
    };
  }

  async #buildReturnValue(
    rv,
    realm,
    awaitPromise,
    resultOwnership,
    serializationOptions,
    options
  ) {
    let evaluationStatus, exception, result, stack;

    if ("return" in rv) {
      evaluationStatus = EvaluationStatus.Normal;
      if (
        awaitPromise &&
        // Only non-primitive return values are wrapped in Debugger.Object.
        rv.return instanceof Debugger.Object &&
        rv.return.isPromise
      ) {
        try {
          // Force wrapping the promise resolution result in a Debugger.Object
          // wrapper for consistency with the synchronous codepath.
          const asyncResult = await rv.return.unsafeDereference();
          result = realm.globalObjectReference.makeDebuggeeValue(asyncResult);
        } catch (asyncException) {
          evaluationStatus = EvaluationStatus.Throw;
          exception = realm.globalObjectReference.makeDebuggeeValue(
            asyncException
          );
          stack = rv.return.promiseResolutionSite;
        }
      } else {
        // rv.return is a Debugger.Object or a primitive.
        result = rv.return;
      }
    } else if ("throw" in rv) {
      // rv.throw will be set if the evaluation synchronously failed, either if
      // the script contains a syntax error or throws an exception.
      evaluationStatus = EvaluationStatus.Throw;
      exception = rv.throw;
      stack = rv.stack;
    }

    switch (evaluationStatus) {
      case EvaluationStatus.Normal:
        return {
          evaluationStatus,
          result: lazy.serialize(
            this.#toRawObject(result),
            serializationOptions,
            resultOwnership,
            new Map(),
            realm,
            options
          ),
          realmId: realm.id,
        };
      case EvaluationStatus.Throw:
        return {
          evaluationStatus,
          exceptionDetails: this.#buildExceptionDetails(
            exception,
            stack,
            realm,
            resultOwnership,
            options
          ),
          realmId: realm.id,
        };
      default:
        throw new lazy.error.UnsupportedOperationError(
          `Unsupported completion value for expression evaluation`
        );
    }
  }

  /**
   * Emit "script.message" event with provided data.
   *
   * @param {Realm} realm
   * @param {ChannelProperties} channelProperties
   * @param {RemoteValue} message
   */
  #emitScriptMessage = (realm, channelProperties, message) => {
    const {
      channel,
      ownership: ownershipType = lazy.OwnershipModel.None,
      serializationOptions,
    } = channelProperties;

    const data = lazy.serialize(
      this.#toRawObject(message),
      lazy.setDefaultSerializationOptions(serializationOptions),
      ownershipType,
      new Map(),
      realm
    );

    this.emitEvent("script.message", {
      channel,
      data,
      source: this.#getSource(realm),
    });
  };

  #evaluatePreloadScripts() {
    let resolveBlockerPromise;
    const blockerPromise = new Promise(resolve => {
      resolveBlockerPromise = resolve;
    });

    // Block script parsing.
    this.messageHandler.window.document.blockParsing(blockerPromise);
    for (const script of this.#preloadScripts.values()) {
      const {
        arguments: commandArguments,
        functionDeclaration,
        sandbox,
      } = script;
      const realm = this.#getRealmFromSandboxName(sandbox);
      const deserializedArguments = commandArguments.map(arg =>
        lazy.deserialize(realm, arg, {
          emitScriptMessage: this.#emitScriptMessage,
        })
      );
      const rv = realm.executeInGlobalWithBindings(
        functionDeclaration,
        deserializedArguments
      );

      if ("throw" in rv) {
        const exception = this.#toRawObject(rv.throw);
        realm.reportError(lazy.stringify(exception), rv.stack);
      }
    }

    // Continue script parsing.
    resolveBlockerPromise();
  }

  #getRealm(realmId, sandboxName) {
    if (realmId === null) {
      return this.#getRealmFromSandboxName(sandboxName);
    }

    if (this.#defaultRealm.id == realmId) {
      return this.#defaultRealm;
    }

    const sandboxRealm = Array.from(this.#realms.values()).find(
      realm => realm.id === realmId
    );

    if (sandboxRealm) {
      return sandboxRealm;
    }

    throw new lazy.error.NoSuchFrameError(`Realm with id ${realmId} not found`);
  }

  #getRealmFromSandboxName(sandboxName) {
    if (sandboxName === null || sandboxName === "") {
      return this.#defaultRealm;
    }

    if (this.#realms.has(sandboxName)) {
      return this.#realms.get(sandboxName);
    }

    const realm = new lazy.WindowRealm(this.messageHandler.window, {
      sandboxName,
    });

    this.#realms.set(sandboxName, realm);

    return realm;
  }

  #getSource(realm) {
    return {
      realm: realm.id,
      context: this.messageHandler.context,
    };
  }

  #startObserving() {
    if (!this.#observerListening) {
      Services.obs.addObserver(this, "document-element-inserted");
      this.#observerListening = true;
    }
  }

  #stopObserving() {
    if (this.#observerListening) {
      Services.obs.removeObserver(this, "document-element-inserted");
      this.#observerListening = false;
    }
  }

  #toRawObject(maybeDebuggerObject) {
    if (maybeDebuggerObject instanceof Debugger.Object) {
      // Retrieve the referent for the provided Debugger.object.
      // See https://firefox-source-docs.mozilla.org/devtools-user/debugger-api/debugger.object/index.html
      const rawObject = maybeDebuggerObject.unsafeDereference();

      // TODO: Getters for Maps and Sets iterators return "Opaque" objects and
      // are not iterable. RemoteValue.jsm' serializer should handle calling
      // waiveXrays on Maps/Sets/... and then unwaiveXrays on entries but since
      // we serialize with maxDepth=1, calling waiveXrays once on the root
      // object allows to return correctly serialized values.
      return Cu.waiveXrays(rawObject);
    }

    // If maybeDebuggerObject was not a Debugger.Object, it is a primitive value
    // which can be used as is.
    return maybeDebuggerObject;
  }

  /**
   * Call a function in the current window global.
   *
   * @param {object} options
   * @param {boolean} options.awaitPromise
   *     Determines if the command should wait for the return value of the
   *     expression to resolve, if this return value is a Promise.
   * @param {Array<RemoteValue>=} options.commandArguments
   *     The arguments to pass to the function call.
   * @param {string} options.functionDeclaration
   *     The body of the function to call.
   * @param {string=} options.realmId
   *     The id of the realm.
   * @param {OwnershipModel} options.resultOwnership
   *     The ownership model to use for the results of this evaluation.
   * @param {string=} options.sandbox
   *     The name of the sandbox.
   * @param {SerializationOptions=} options.serializationOptions
   *     An object which holds the information of how the result of evaluation
   *     in case of ECMAScript objects should be serialized.
   * @param {RemoteValue=} options.thisParameter
   *     The value of the this keyword for the function call.
   *
   * @returns {object}
   *     - evaluationStatus {EvaluationStatus} One of "normal", "throw".
   *     - exceptionDetails {ExceptionDetails=} the details of the exception if
   *     the evaluation status was "throw".
   *     - result {RemoteValue=} the result of the evaluation serialized as a
   *     RemoteValue if the evaluation status was "normal".
   */
  async callFunctionDeclaration(options) {
    const {
      awaitPromise,
      commandArguments = null,
      functionDeclaration,
      realmId = null,
      resultOwnership,
      sandbox: sandboxName = null,
      serializationOptions,
      thisParameter = null,
    } = options;

    const realm = this.#getRealm(realmId, sandboxName);
    const nodeCache = this.nodeCache;

    const deserializedArguments =
      commandArguments !== null
        ? commandArguments.map(arg =>
            lazy.deserialize(realm, arg, {
              emitScriptMessage: this.#emitScriptMessage,
              nodeCache,
            })
          )
        : [];

    const deserializedThis =
      thisParameter !== null
        ? lazy.deserialize(realm, thisParameter, {
            emitScriptMessage: this.#emitScriptMessage,
            nodeCache,
          })
        : null;

    const rv = realm.executeInGlobalWithBindings(
      functionDeclaration,
      deserializedArguments,
      deserializedThis
    );

    return this.#buildReturnValue(
      rv,
      realm,
      awaitPromise,
      resultOwnership,
      serializationOptions,
      {
        nodeCache,
      }
    );
  }

  /**
   * Delete the provided handles from the realm corresponding to the provided
   * sandbox name.
   *
   * @param {object=} options
   * @param {Array<string>} options.handles
   *     Array of handle ids to disown.
   * @param {string=} options.realmId
   *     The id of the realm.
   * @param {string=} options.sandbox
   *     The name of the sandbox.
   */
  disownHandles(options) {
    const { handles, realmId = null, sandbox: sandboxName = null } = options;
    const realm = this.#getRealm(realmId, sandboxName);
    for (const handle of handles) {
      realm.removeObjectHandle(handle);
    }
  }

  /**
   * Evaluate a provided expression in the current window global.
   *
   * @param {object} options
   * @param {boolean} options.awaitPromise
   *     Determines if the command should wait for the return value of the
   *     expression to resolve, if this return value is a Promise.
   * @param {string} options.expression
   *     The expression to evaluate.
   * @param {string=} options.realmId
   *     The id of the realm.
   * @param {OwnershipModel} options.resultOwnership
   *     The ownership model to use for the results of this evaluation.
   * @param {string=} options.sandbox
   *     The name of the sandbox.
   *
   * @returns {object}
   *     - evaluationStatus {EvaluationStatus} One of "normal", "throw".
   *     - exceptionDetails {ExceptionDetails=} the details of the exception if
   *     the evaluation status was "throw".
   *     - result {RemoteValue=} the result of the evaluation serialized as a
   *     RemoteValue if the evaluation status was "normal".
   */
  async evaluateExpression(options) {
    const {
      awaitPromise,
      expression,
      realmId = null,
      resultOwnership,
      sandbox: sandboxName = null,
      serializationOptions,
    } = options;

    const realm = this.#getRealm(realmId, sandboxName);

    const rv = realm.executeInGlobal(expression);

    return this.#buildReturnValue(
      rv,
      realm,
      awaitPromise,
      resultOwnership,
      serializationOptions,
      {
        nodeCache: this.nodeCache,
      }
    );
  }

  /**
   * Get realms for the current window global.
   *
   * @returns {Array<object>}
   *     - context {BrowsingContext} The browsing context, associated with the realm.
   *     - id {string} The realm unique identifier.
   *     - origin {string} The serialization of an origin.
   *     - sandbox {string=} The name of the sandbox.
   *     - type {RealmType.Window} The window realm type.
   */
  getWindowRealms() {
    return [this.#defaultRealm, ...this.#realms.values()].map(realm =>
      realm.getInfo()
    );
  }

  /**
   * Internal commands
   */

  _applySessionData(params) {
    // We only care about updates coming on context creation.
    if (params.category === "preload-script" && params.initial) {
      this.#preloadScripts = new Set();
      for (const item of params.sessionData) {
        if (this.messageHandler.matchesContext(item.contextDescriptor)) {
          this.#preloadScripts.add(item.value);
        }
      }

      if (this.#preloadScripts.size) {
        this.#startObserving();
      }
    }
  }
}

export const script = ScriptModule;
