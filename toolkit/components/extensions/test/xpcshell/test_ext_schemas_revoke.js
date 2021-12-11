/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { Schemas } = ChromeUtils.import("resource://gre/modules/Schemas.jsm");

let { SchemaAPIInterface } = ExtensionCommon;

const global = this;

let json = [
  {
    namespace: "revokableNs",

    permissions: ["revokableNs"],

    properties: {
      stringProp: {
        type: "string",
        writable: true,
      },

      revokableStringProp: {
        type: "string",
        permissions: ["revokableProp"],
        writable: true,
      },

      submoduleProp: {
        $ref: "submodule",
      },

      revokableSubmoduleProp: {
        $ref: "submodule",
        permissions: ["revokableProp"],
      },
    },

    types: [
      {
        id: "submodule",
        type: "object",
        functions: [
          {
            name: "sub_foo",
            type: "function",
            parameters: [],
            returns: { type: "integer" },
          },
        ],
      },
    ],

    functions: [
      {
        name: "func",
        type: "function",
        parameters: [],
      },

      {
        name: "revokableFunc",
        type: "function",
        parameters: [],
        permissions: ["revokableFunc"],
      },
    ],

    events: [
      {
        name: "onEvent",
        type: "function",
      },

      {
        name: "onRevokableEvent",
        type: "function",
        permissions: ["revokableEvent"],
      },
    ],
  },
];

let recorded = [];

function record(...args) {
  recorded.push(args);
}

function verify(expected) {
  for (let [i, rec] of expected.entries()) {
    Assert.deepEqual(recorded[i], rec, `Record ${i} matches`);
  }

  equal(recorded.length, expected.length, "Got expected number of records");

  recorded.length = 0;
}

registerCleanupFunction(() => {
  equal(recorded.length, 0, "No unchecked recorded events at shutdown");
});

let permissions = new Set();

class APIImplementation extends SchemaAPIInterface {
  constructor(namespace, name) {
    super();
    this.namespace = namespace;
    this.name = name;
  }

  record(method, args) {
    record(method, this.namespace, this.name, args);
  }

  revoke(...args) {
    this.record("revoke", args);
  }

  callFunction(...args) {
    this.record("callFunction", args);
    if (this.name === "sub_foo") {
      return 13;
    }
  }

  callFunctionNoReturn(...args) {
    this.record("callFunctionNoReturn", args);
  }

  getProperty(...args) {
    this.record("getProperty", args);
  }

  setProperty(...args) {
    this.record("setProperty", args);
  }

  addListener(...args) {
    this.record("addListener", args);
  }

  removeListener(...args) {
    this.record("removeListener", args);
  }

  hasListener(...args) {
    this.record("hasListener", args);
  }
}

let context = {
  manifestVersion: 2,
  cloneScope: global,

  permissionsChanged: null,

  setPermissionsChangedCallback(callback) {
    this.permissionsChanged = callback;
  },

  hasPermission(permission) {
    return permissions.has(permission);
  },

  isPermissionRevokable(permission) {
    return permission.startsWith("revokable");
  },

  getImplementation(namespace, name) {
    return new APIImplementation(namespace, name);
  },

  shouldInject() {
    return true;
  },
};

function ignoreError(fn) {
  try {
    fn();
  } catch (e) {
    // Meh.
  }
}

add_task(async function() {
  let url = "data:," + JSON.stringify(json);
  await Schemas.load(url);

  let root = {};
  Schemas.inject(root, context);
  equal(recorded.length, 0, "No recorded events");

  let listener = () => {};
  let captured = {};

  function checkRecorded() {
    let possible = [
      ["revokableNs", ["getProperty", "revokableNs", "stringProp", []]],
      [
        "revokableProp",
        ["getProperty", "revokableNs", "revokableStringProp", []],
      ],

      [
        "revokableNs",
        ["setProperty", "revokableNs", "stringProp", ["stringProp"]],
      ],
      [
        "revokableProp",
        [
          "setProperty",
          "revokableNs",
          "revokableStringProp",
          ["revokableStringProp"],
        ],
      ],

      ["revokableNs", ["callFunctionNoReturn", "revokableNs", "func", [[]]]],
      [
        "revokableFunc",
        ["callFunctionNoReturn", "revokableNs", "revokableFunc", [[]]],
      ],

      [
        "revokableNs",
        ["callFunction", "revokableNs.submoduleProp", "sub_foo", [[]]],
      ],
      [
        "revokableProp",
        ["callFunction", "revokableNs.revokableSubmoduleProp", "sub_foo", [[]]],
      ],

      [
        "revokableNs",
        ["addListener", "revokableNs", "onEvent", [listener, []]],
      ],
      ["revokableNs", ["removeListener", "revokableNs", "onEvent", [listener]]],
      ["revokableNs", ["hasListener", "revokableNs", "onEvent", [listener]]],

      [
        "revokableEvent",
        ["addListener", "revokableNs", "onRevokableEvent", [listener, []]],
      ],
      [
        "revokableEvent",
        ["removeListener", "revokableNs", "onRevokableEvent", [listener]],
      ],
      [
        "revokableEvent",
        ["hasListener", "revokableNs", "onRevokableEvent", [listener]],
      ],
    ];

    let expected = [];
    if (permissions.has("revokableNs")) {
      for (let [perm, recording] of possible) {
        if (!perm || permissions.has(perm)) {
          expected.push(recording);
        }
      }
    }

    verify(expected);
  }

  function check() {
    info(`Check normal access (permissions: [${Array.from(permissions)}])`);

    let ns = root.revokableNs;

    void ns.stringProp;
    void ns.revokableStringProp;

    ns.stringProp = "stringProp";
    ns.revokableStringProp = "revokableStringProp";

    ns.func();

    if (ns.revokableFunc) {
      ns.revokableFunc();
    }

    ns.submoduleProp.sub_foo();
    if (ns.revokableSubmoduleProp) {
      ns.revokableSubmoduleProp.sub_foo();
    }

    ns.onEvent.addListener(listener);
    ns.onEvent.removeListener(listener);
    ns.onEvent.hasListener(listener);

    if (ns.onRevokableEvent) {
      ns.onRevokableEvent.addListener(listener);
      ns.onRevokableEvent.removeListener(listener);
      ns.onRevokableEvent.hasListener(listener);
    }

    checkRecorded();
  }

  function capture() {
    info("Capture values");

    let ns = root.revokableNs;

    captured = { ns };
    captured.revokableStringProp = Object.getOwnPropertyDescriptor(
      ns,
      "revokableStringProp"
    );

    captured.revokableSubmoduleProp = ns.revokableSubmoduleProp;
    if (ns.revokableSubmoduleProp) {
      captured.sub_foo = ns.revokableSubmoduleProp.sub_foo;
    }

    captured.revokableFunc = ns.revokableFunc;

    captured.onRevokableEvent = ns.onRevokableEvent;
    if (ns.onRevokableEvent) {
      captured.addListener = ns.onRevokableEvent.addListener;
      captured.removeListener = ns.onRevokableEvent.removeListener;
      captured.hasListener = ns.onRevokableEvent.hasListener;
    }
  }

  function checkCaptured() {
    info(
      `Check captured value access (permissions: [${Array.from(permissions)}])`
    );

    let { ns } = captured;

    void ns.stringProp;
    ignoreError(() => captured.revokableStringProp.get());
    if (!permissions.has("revokableProp")) {
      void ns.revokableStringProp;
    }

    ns.stringProp = "stringProp";
    ignoreError(() => captured.revokableStringProp.set("revokableStringProp"));
    if (!permissions.has("revokableProp")) {
      ns.revokableStringProp = "revokableStringProp";
    }

    ignoreError(() => ns.func());
    ignoreError(() => captured.revokableFunc());
    if (!permissions.has("revokableFunc")) {
      ignoreError(() => ns.revokableFunc());
    }

    ignoreError(() => ns.submoduleProp.sub_foo());

    ignoreError(() => captured.sub_foo());
    if (!permissions.has("revokableProp")) {
      ignoreError(() => captured.revokableSubmoduleProp.sub_foo());
      ignoreError(() => ns.revokableSubmoduleProp.sub_foo());
    }

    ignoreError(() => ns.onEvent.addListener(listener));
    ignoreError(() => ns.onEvent.removeListener(listener));
    ignoreError(() => ns.onEvent.hasListener(listener));

    ignoreError(() => captured.addListener(listener));
    ignoreError(() => captured.removeListener(listener));
    ignoreError(() => captured.hasListener(listener));
    if (!permissions.has("revokableEvent")) {
      ignoreError(() => captured.onRevokableEvent.addListener(listener));
      ignoreError(() => captured.onRevokableEvent.removeListener(listener));
      ignoreError(() => captured.onRevokableEvent.hasListener(listener));

      ignoreError(() => ns.onRevokableEvent.addListener(listener));
      ignoreError(() => ns.onRevokableEvent.removeListener(listener));
      ignoreError(() => ns.onRevokableEvent.hasListener(listener));
    }

    checkRecorded();
  }

  permissions.add("revokableNs");
  permissions.add("revokableProp");
  permissions.add("revokableFunc");
  permissions.add("revokableEvent");

  check();
  capture();
  checkCaptured();

  permissions.delete("revokableProp");
  context.permissionsChanged();
  verify([
    ["revoke", "revokableNs", "revokableStringProp", []],
    ["revoke", "revokableNs.revokableSubmoduleProp", "sub_foo", []],
  ]);

  check();
  checkCaptured();

  permissions.delete("revokableFunc");
  context.permissionsChanged();
  verify([["revoke", "revokableNs", "revokableFunc", []]]);

  check();
  checkCaptured();

  permissions.delete("revokableEvent");
  context.permissionsChanged();

  verify([["revoke", "revokableNs", "onRevokableEvent", []]]);

  check();
  checkCaptured();

  permissions.delete("revokableNs");
  context.permissionsChanged();
  verify([
    ["revoke", "revokableNs", "stringProp", []],
    ["revoke", "revokableNs", "func", []],
    ["revoke", "revokableNs.submoduleProp", "sub_foo", []],
    ["revoke", "revokableNs", "onEvent", []],
  ]);

  checkCaptured();

  permissions.add("revokableNs");
  permissions.add("revokableProp");
  permissions.add("revokableFunc");
  permissions.add("revokableEvent");
  context.permissionsChanged();

  check();
  capture();
  checkCaptured();

  permissions.delete("revokableProp");
  permissions.delete("revokableFunc");
  permissions.delete("revokableEvent");
  context.permissionsChanged();
  verify([
    ["revoke", "revokableNs", "revokableStringProp", []],
    ["revoke", "revokableNs", "revokableFunc", []],
    ["revoke", "revokableNs.revokableSubmoduleProp", "sub_foo", []],
    ["revoke", "revokableNs", "onRevokableEvent", []],
  ]);

  check();
  checkCaptured();

  permissions.add("revokableProp");
  permissions.add("revokableFunc");
  permissions.add("revokableEvent");
  context.permissionsChanged();

  check();
  capture();
  checkCaptured();

  permissions.delete("revokableNs");
  context.permissionsChanged();
  verify([
    ["revoke", "revokableNs", "stringProp", []],
    ["revoke", "revokableNs", "revokableStringProp", []],
    ["revoke", "revokableNs", "func", []],
    ["revoke", "revokableNs", "revokableFunc", []],
    ["revoke", "revokableNs.submoduleProp", "sub_foo", []],
    ["revoke", "revokableNs.revokableSubmoduleProp", "sub_foo", []],
    ["revoke", "revokableNs", "onEvent", []],
    ["revoke", "revokableNs", "onRevokableEvent", []],
  ]);

  equal(root.revokableNs, undefined, "Namespace is not defined");
  checkCaptured();
});

add_task(async function test_neuter() {
  context.permissionsChanged = null;

  let root = {};
  Schemas.inject(root, context);
  equal(recorded.length, 0, "No recorded events");

  permissions.add("revokableNs");
  permissions.add("revokableProp");
  permissions.add("revokableFunc");
  permissions.add("revokableEvent");

  let ns = root.revokableNs;
  let { submoduleProp } = ns;

  let lazyGetter = Object.getOwnPropertyDescriptor(submoduleProp, "sub_foo");

  permissions.delete("revokableNs");
  context.permissionsChanged();
  verify([]);

  equal(root.revokableNs, undefined, "Should have no revokableNs");
  equal(ns.submoduleProp, undefined, "Should have no ns.submoduleProp");

  equal(submoduleProp.sub_foo, undefined, "No sub_foo");
  lazyGetter.get.call(submoduleProp);
  equal(submoduleProp.sub_foo, undefined, "No sub_foo");
});
