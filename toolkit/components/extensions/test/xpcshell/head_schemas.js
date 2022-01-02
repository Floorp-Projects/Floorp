"use strict";

/* exported Schemas, LocalAPIImplementation, SchemaAPIInterface, getContextWrapper */

const { Schemas } = ChromeUtils.import("resource://gre/modules/Schemas.jsm");

const { ExtensionCommon } = ChromeUtils.import(
  "resource://gre/modules/ExtensionCommon.jsm"
);

let { LocalAPIImplementation, SchemaAPIInterface } = ExtensionCommon;

const contextCloneScope = this;

class TallyingAPIImplementation extends SchemaAPIInterface {
  constructor(context, namespace, name) {
    super();
    this.namespace = namespace;
    this.name = name;
    this.context = context;
  }

  callFunction(args) {
    this.context.tally("call", this.namespace, this.name, args);
    if (this.name === "sub_foo") {
      return 13;
    }
  }

  callFunctionNoReturn(args) {
    this.context.tally("call", this.namespace, this.name, args);
  }

  getProperty() {
    this.context.tally("get", this.namespace, this.name);
  }

  setProperty(value) {
    this.context.tally("set", this.namespace, this.name, value);
  }

  addListener(listener, args) {
    this.context.tally("addListener", this.namespace, this.name, [
      listener,
      args,
    ]);
  }

  removeListener(listener) {
    this.context.tally("removeListener", this.namespace, this.name, [listener]);
  }

  hasListener(listener) {
    this.context.tally("hasListener", this.namespace, this.name, [listener]);
  }
}

function getContextWrapper(manifestVersion = 2) {
  return {
    url: "moz-extension://b66e3509-cdb3-44f6-8eb8-c8b39b3a1d27/",

    cloneScope: contextCloneScope,

    manifestVersion,

    permissions: new Set(),
    tallied: null,
    talliedErrors: [],

    tally(kind, ns, name, args) {
      this.tallied = [kind, ns, name, args];
    },

    verify(...args) {
      Assert.equal(JSON.stringify(this.tallied), JSON.stringify(args));
      this.tallied = null;
    },

    checkErrors(errors) {
      let { talliedErrors } = this;
      Assert.equal(
        talliedErrors.length,
        errors.length,
        "Got expected number of errors"
      );
      for (let [i, error] of errors.entries()) {
        Assert.ok(
          i in talliedErrors && String(talliedErrors[i]).includes(error),
          `${JSON.stringify(error)} is a substring of error ${JSON.stringify(
            talliedErrors[i]
          )}`
        );
      }

      talliedErrors.length = 0;
    },

    checkLoadURL(url) {
      return !url.startsWith("chrome:");
    },

    preprocessors: {
      localize(value, context) {
        return value.replace(
          /__MSG_(.*?)__/g,
          (m0, m1) => `${m1.toUpperCase()}`
        );
      },
    },

    logError(message) {
      this.talliedErrors.push(message);
    },

    hasPermission(permission) {
      return this.permissions.has(permission);
    },

    shouldInject(ns, name, allowedContexts) {
      return name != "do-not-inject";
    },

    getImplementation(namespace, name) {
      return new TallyingAPIImplementation(this, namespace, name);
    },
  };
}
