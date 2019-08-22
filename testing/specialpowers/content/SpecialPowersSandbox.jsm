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

var EXPORTED_SYMBOLS = ["SpecialPowersSandbox"];

ChromeUtils.defineModuleGetter(
  this,
  "Assert",
  "resource://testing-common/Assert.jsm"
);

class SpecialPowersSandbox {
  constructor(name, reportCallback, opts = {}) {
    this.name = name;
    this.reportCallback = reportCallback;

    this._Assert = null;

    this.sandbox = Cu.Sandbox(
      Cu.getGlobalForObject({}),
      Object.assign(
        { wantGlobalProperties: ["ChromeUtils"] },
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
  }

  static getCallerInfo(frame) {
    return {
      filename: frame.filename,
      lineNumber: frame.lineNumber,
    };
  }

  get Assert() {
    if (!this._Assert) {
      this._Assert = new Assert((err, message, stack) => {
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
