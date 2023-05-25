/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This modules handles creating and provisioning Sandboxes for
 * executing cross-process code from SpecialPowers. This allows all such
 * sandboxes to have a similar environment, and in particular allows
 * them to run test assertions in the target process and propagate
 * results back to the caller.
 */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Assert: "resource://testing-common/Assert.sys.mjs",
});

// Note: When updating the set of globals exposed to sandboxes by
// default, please also update the ESLint plugin rule defined in
// import-content-task-globals.js.
const SANDBOX_GLOBALS = [
  "Blob",
  "ChromeUtils",
  "FileReader",
  "TextDecoder",
  "TextEncoder",
  "URL",
];
const EXTRA_IMPORTS = {
  EventUtils: "resource://testing-common/SpecialPowersEventUtils.sys.mjs",
};

let expectFail = false;
function expectingFail(fn) {
  try {
    expectFail = true;
    fn();
  } finally {
    expectFail = false;
  }
}

export class SpecialPowersSandbox {
  constructor(name, reportCallback, opts = {}) {
    this.name = name;
    this.reportCallback = reportCallback;

    this._Assert = null;

    this.sandbox = Cu.Sandbox(
      Cu.getGlobalForObject({}),
      Object.assign(
        { wantGlobalProperties: SANDBOX_GLOBALS },
        opts.sandboxOptions
      )
    );

    for (let prop of ["assert", "Assert"]) {
      Object.defineProperty(this.sandbox, prop, {
        get: () => {
          return this.Assert;
        },
        enumerable: true,
        configurable: true,
      });
    }

    let imports = {
      ...EXTRA_IMPORTS,
      ...opts.imports,
    };
    // We explicitly want these directly in the sandbox, and we aren't going
    // to be using the globals within this file.
    // eslint-disable-next-line mozilla/lazy-getter-object-name
    ChromeUtils.defineESModuleGetters(this.sandbox, imports);

    // Note: When updating the set of globals exposed to sandboxes by
    // default, please also update the ESLint plugin rule defined in
    // import-content-task-globals.js.
    Object.assign(this.sandbox, {
      BrowsingContext,
      InspectorUtils,
      ok: (...args) => {
        this.Assert.ok(...args);
      },
      is: (...args) => {
        this.Assert.equal(...args);
      },
      isnot: (...args) => {
        this.Assert.notEqual(...args);
      },
      todo: (...args) => {
        expectingFail(() => this.Assert.ok(...args));
      },
      todo_is: (...args) => {
        expectingFail(() => this.Assert.equal(...args));
      },
      info: info => {
        this.reportCallback({ info });
      },
    });
  }

  get Assert() {
    if (!this._Assert) {
      this._Assert = new lazy.Assert((err, message, stack) => {
        this.report(err, message, stack);
      });
    }
    return this._Assert;
  }

  report(err, name, stack) {
    let diag;
    if (err) {
      diag =
        `got ${uneval(err.actual)}, expected ${uneval(err.expected)} ` +
        `(operator ${err.operator})`;
    }

    this.reportCallback({
      name,
      diag,
      passed: !err,
      stack: stack && stack.formattedStack,
      expectFail,
    });
  }

  execute(task, args, caller) {
    let func = Cu.evalInSandbox(
      `(${task})`,
      this.sandbox,
      undefined,
      caller.filename,
      caller.lineNumber
    );
    return func(...args);
  }
}
