/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * A facade around @cfworker/json-schema that provides additional formats and
 * convenience methods whil executing inside a sandbox.
 */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const sandbox = new Cu.Sandbox(null, {
  wantComponents: false,
  wantGlobalProperties: ["URL"],
});

Services.scriptloader.loadSubScript(
  "chrome://global/content/third_party/cfworker/json-schema.js",
  sandbox
);

/**
 * A JSON Schema string format for URLs intended to go through Services.urlFormatter.
 */
Cu.exportFunction(
  function validateMozUrlFormat(input) {
    try {
      const formatted = Services.urlFormatter.formatURL(input);
      return Cu.waiveXrays(sandbox.fastFormat).uri(formatted);
    } catch {
      return false;
    }
  },
  sandbox.fastFormat,
  { defineAs: "moz-url-format" }
);

/**
 * A JSONSchema validator that performs validation inside a sandbox.
 */
class Validator {
  #inner;

  /**
   * Create a new validator.
   *
   * @param {object} schema The schema to validate with.
   * @param {string} draft  The draft to validate against. Should
   *                        be one of "4", "7", "2019-09".
   * @param {boolean} shortCircuit  Whether or not the validator should return
   *                                after a single error occurs.
   */
  constructor(schema, draft = "2019-09", shortCircuit = true) {
    this.#inner = Cu.waiveXrays(
      new sandbox.Validator(Cu.cloneInto(schema, sandbox), draft, shortCircuit)
    );
  }

  /**
   * Validate the instance against the known schemas.
   *
   * @param {object} instance  The instance to validate.
   *
   * @return {object}  An object with |valid| and |errors| keys that indicates
   *                   the success of validation.
   */
  validate(instance) {
    return this.#inner.validate(Cu.cloneInto(instance, sandbox));
  }

  /**
   * Add a schema to the validator.
   *
   * @param {object} schema  A JSON schema object.
   * @param {string} id  An optional ID to identify the schema if it does not
   *                     provide an |$id| field.
   */
  addSchema(schema, id) {
    this.#inner.addSchema(Cu.cloneInto(schema, sandbox), id);
  }
}

/**
 * A wrapper around validate that provides some options as an object
 * instead of positional arguments.
 *
 * @param {object} instance  The instance to validate.
 * @param {object} schema  The JSON schema to validate against.
 * @param {object} options  Options for the validator.
 * @param {string} options.draft  The draft to validate against. Should
 *                                be one of "4", "7", "2019-09".
 * @param {boolean} options.shortCircuit  Whether or not the validator should
 *                                        return after a single error occurs.
 *
 * @returns {object} An object with |valid| and |errors| keys that indicates the
 *                   success of validation.
 */
function validate(
  instance,
  schema,
  { draft = "2019-09", shortCircuit = true } = {}
) {
  const clonedSchema = Cu.cloneInto(schema, sandbox);

  return sandbox.validate(
    Cu.cloneInto(instance, sandbox),
    clonedSchema,
    draft,
    sandbox.dereference(clonedSchema),
    shortCircuit
  );
}

const EXPORTED_SYMBOLS = ["Validator", "validate", "sandbox"];
