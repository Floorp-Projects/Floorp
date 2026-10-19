// SPDX-License-Identifier: MPL-2.0
// Self-authored public extension operations shared unchanged by DOC and Worker.
(() => {
  const realm = typeof document === "object" ? "document" : "worker";
  const messages = [];
  const ports = [];
  let expectation = null;
  const portResponses = new Map();
  const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
  const errorInfo = (error) => ({
    type: typeof error,
    name: String(error?.name),
    message: String(error?.message ?? error),
  });
  function describe(value, seen = new WeakSet()) {
    const type = typeof value;
    if (value === null) {
      return { type: "object", tag: "[object Null]", value: null };
    }
    if (type === "number") {
      return {
        type,
        value: Number.isNaN(value)
          ? "NaN"
          : value === Infinity
          ? "+Infinity"
          : value === -Infinity
          ? "-Infinity"
          : Object.is(value, -0)
          ? "-0"
          : value,
      };
    }
    if (type === "undefined" || type === "symbol" || type === "function") {
      return { type, text: String(value) };
    }
    if (type === "bigint") return { type, text: String(value) };
    if (type !== "object") return { type, value };
    const tag = Object.prototype.toString.call(value);
    if (seen.has(value)) return { type, tag, cycle: true };
    seen.add(value);
    const keys = Object.keys(value);
    const entries = keys.map((key) => [key, describe(value[key], seen)]);
    seen.delete(value);
    return {
      type,
      tag,
      keys,
      ...(Array.isArray(value) ? { length: value.length } : {}),
      entries,
    };
  }
  const caseIds = [
    "undefined",
    "null",
    "true",
    "false",
    "string",
    "unicode",
    "number",
    "negative-zero",
    "nan",
    "infinity",
    "negative-infinity",
    "nested-primitives",
    "cycle",
    "bigint",
    "nested-bigint",
    "array-bigint",
    "date",
    "invalid-date",
    "map",
    "set",
    "typed-array",
    "error",
    "function",
    "symbol",
    "nested-unsupported",
    "array-unsupported",
    "tojson-object",
    "tojson-undefined",
    "tojson-throw",
    "tojson-bigint",
    "getter",
    "getter-throw",
  ];
  function makeCase(id) {
    const state = {
      toJSONCalls: 0,
      getterCalls: 0,
      functionCalls: 0,
      functionArgumentTypes: [],
      receiverThis: [],
      keys: [],
    };
    let value;
    switch (id) {
      case "undefined":
        value = undefined;
        break;
      case "null":
        value = null;
        break;
      case "true":
        value = true;
        break;
      case "false":
        value = false;
        break;
      case "string":
        value = "owned-message";
        break;
      case "unicode":
        value = "Floorp 日本語 🚀";
        break;
      case "number":
        value = 42.25;
        break;
      case "negative-zero":
        value = -0;
        break;
      case "nan":
        value = NaN;
        break;
      case "infinity":
        value = Infinity;
        break;
      case "negative-infinity":
        value = -Infinity;
        break;
      case "nested-primitives":
        value = {
          nil: null,
          str: "owned",
          bool: false,
          number: 17,
          list: [1, "two", true],
        };
        break;
      case "cycle":
        value = {};
        value.self = value;
        break;
      case "bigint":
        value = 12n;
        break;
      case "nested-bigint":
        value = { nested: 12n };
        break;
      case "array-bigint":
        value = [12n];
        break;
      case "date":
        value = new Date("2026-10-19T12:34:56.000Z");
        break;
      case "invalid-date":
        value = new Date(NaN);
        break;
      case "map":
        value = new Map([["key", "value"]]);
        break;
      case "set":
        value = new Set(["owned"]);
        break;
      case "typed-array":
        value = new Uint8Array([3, 7, 255]);
        break;
      case "error":
        value = new Error("owned Error payload");
        break;
      case "function":
        value = function ownedPayload(...args) {
          state.functionCalls++;
          state.functionArgumentTypes.push(args.map((value) => typeof value));
        };
        break;
      case "symbol":
        value = Symbol("owned payload");
        break;
      case "nested-unsupported":
        value = {
          before: "kept",
          undef: undefined,
          fn() {},
          sym: Symbol("nested"),
          after: null,
        };
        break;
      case "array-unsupported":
        value = [
          "kept",
          undefined,
          function nested() {},
          Symbol("array"),
          null,
        ];
        break;
      case "tojson-object":
      case "tojson-undefined":
      case "tojson-throw":
      case "tojson-bigint":
        value = {
          label: "original",
          toJSON(key) {
            state.toJSONCalls++;
            state.receiverThis.push(this === value);
            state.keys.push(key);
            if (id === "tojson-undefined") return undefined;
            if (id === "tojson-throw") {
              throw new RangeError(
                "owned toJSON failure",
              );
            }
            if (id === "tojson-bigint") return 7n;
            return { transformed: true, unicode: "変換 🚀" };
          },
        };
        break;
      case "getter":
      case "getter-throw":
        value = {
          label: "original",
          get selected() {
            state.getterCalls++;
            state.receiverThis.push(this === value);
            if (id === "getter-throw") {
              throw new SyntaxError(
                "owned getter failure",
              );
            }
            return { materialized: true, count: state.getterCalls };
          },
        };
        break;
      default:
        throw new Error("Unknown authored case: " + id);
    }
    return { value, state };
  }

  chrome.runtime.onMessage.addListener((value, sender, sendResponse) => {
    if (!expectation) return undefined;
    const request = expectation;
    expectation = null;
    const received = {
      id: request.id,
      realm,
      runtimeId: chrome.runtime.id,
      senderId: sender.id,
      observed: describe(value),
    };
    messages.push(received);
    if (request.kind === "response") {
      const payload = makeCase(request.caseId);
      received.payloadState = payload.state;
      received.mode = request.mode;
      if (request.mode === "promise") return Promise.resolve(payload.value);
      try {
        const result = sendResponse(payload.value);
        received.responseReturn = describe(result);
      } catch (error) {
        received.responseThrow = errorInfo(error);
      }
      return undefined;
    }
    sendResponse({ oracleAck: request.id, realm, observed: received.observed });
    return undefined;
  });
  chrome.runtime.onConnect.addListener((port) => {
    if (!port.name.startsWith("owned-json-")) return;
    const received = {
      name: port.name,
      realm,
      runtimeId: chrome.runtime.id,
      messages: [],
      disconnected: false,
    };
    ports.push(received);
    port.onMessage.addListener((value) => {
      received.messages.push(describe(value));
      const response = portResponses.get(port.name);
      if (response) {
        portResponses.delete(port.name);
        const payload = makeCase(response.caseId);
        received.payloadState = payload.state;
        try {
          const result = port.postMessage(payload.value);
          received.responseReturn = describe(result);
        } catch (error) {
          received.responseThrow = errorInfo(error);
        }
      } else {
        port.postMessage({
          oracleAck: port.name,
          realm,
          observed: describe(value),
        });
      }
    });
    port.onDisconnect.addListener(() => {
      received.disconnected = true;
      received.disconnectError = chrome.runtime.lastError?.message ?? null;
    });
  });

  async function invokeSend(id, value, mode, response = false) {
    const record = {
      id,
      api: "runtime.sendMessage",
      mode,
      realm,
      runtimeId: chrome.runtime.id,
      response,
      outcome: "pending",
    };
    let result;
    try {
      result = mode === "explicit-id"
        ? chrome.runtime.sendMessage(chrome.runtime.id, value)
        : chrome.runtime.sendMessage(value);
      record.returnedType = typeof result;
      record.returnedThenable = typeof result?.then === "function";
    } catch (error) {
      record.outcome = "sync-throw";
      record.error = errorInfo(error);
      return record;
    }
    if (!record.returnedThenable) {
      record.outcome = "returned";
      record.result = describe(result);
      await sleep(30);
      return record;
    }
    await Promise.race([
      Promise.resolve(result).then((value) => {
        record.outcome = "resolved";
        record.result = describe(value);
      }, (error) => {
        record.outcome = "promise-rejection";
        record.error = errorInfo(error);
      }),
      sleep(600).then(() => {
        if (record.outcome === "pending") record.outcome = "timeout";
      }),
    ]);
    return record;
  }
  globalThis.messageJSONOracle = {
    realm,
    runtimeId: chrome.runtime.id,
    caseIds,
    armMessage(id) {
      expectation = { id, kind: "payload" };
    },
    armResponse(id, caseId, mode) {
      expectation = { id, caseId, mode, kind: "response" };
    },
    armPortResponse(name, caseId) {
      portResponses.set(name, { caseId });
    },
    getMessages() {
      return messages;
    },
    getPorts() {
      return ports;
    },
    async send(id, caseId, mode) {
      const payload = makeCase(caseId);
      const result = await invokeSend(id, payload.value, mode);
      return { ...result, caseId, state: payload.state };
    },
    async sendResponseRequest(id, caseId, mode) {
      return {
        ...await invokeSend(id, { responseCase: caseId }, "explicit-id", true),
        caseId,
        responseMode: mode,
      };
    },
    async port(name, caseId, response = false) {
      const payload = response
        ? { value: { responseCase: caseId }, state: null }
        : makeCase(caseId);
      const record = {
        name,
        caseId,
        api: "runtime.Port.postMessage",
        realm,
        runtimeId: chrome.runtime.id,
        response,
        outcome: "pending",
        state: payload.state,
        received: [],
      };
      const port = chrome.runtime.connect({ name });
      port.onMessage.addListener((value) => {
        record.received.push(describe(value));
        record.outcome = "received";
      });
      port.onDisconnect.addListener(() => {
        record.disconnectError = chrome.runtime.lastError?.message ?? null;
        if (record.outcome === "pending") record.outcome = "disconnect";
      });
      try {
        const result = port.postMessage(payload.value);
        record.returned = describe(result);
      } catch (error) {
        record.outcome = "sync-throw";
        record.error = errorInfo(error);
      }
      const deadline = Date.now() + 600;
      while (record.outcome === "pending" && Date.now() < deadline) {
        await sleep(4);
      }
      if (record.outcome === "pending") record.outcome = "timeout";
      port.disconnect();
      return record;
    },
  };
})();
