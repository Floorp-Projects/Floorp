/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * A facade around @cfworker/json-schema that provides additional formats and
 * convenience methods whil executing inside a sandbox.
 */

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

// initialBaseURI defaults to github.com/cfworker, which will be confusing.
Cu.evalInSandbox(
  `this.initialBaseURI = initialBaseURI = new URL("http://mozilla.org");`,
  sandbox
);

/**
 * A JSONSchema validator that performs validation inside a sandbox.
 */
class Validator {
  #inner;
  #draft;

  /**
   * Create a new validator.
   *
   * @param {object} schema The schema to validate with.
   * @param {object} options  Options for the validator.
   * @param {string} options.draft  The draft to validate against. Should be one
   *                                of "4", "6", "7", "2019-09", or "2020-12".
   *
   *                                If the |$schema| key is present in the
   *                                |schema|, it will be used to auto-detect the
   *                                correct version.  Otherwise, 2019-09 will be
   *                                used.
   * @param {boolean} options.shortCircuit  Whether or not the validator should
   *                                        return after a single error occurs.
   */
  constructor(
    schema,
    { draft = detectSchemaDraft(schema), shortCircuit = true } = {}
  ) {
    this.#draft = draft;
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
    const draft = detectSchemaDraft(schema, undefined);
    if (draft && this.#draft != draft) {
      Cu.reportError(
        `Adding a draft "${draft}" schema to a draft "${
          this.#draft
        }" validator.`
      );
    }
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
 *                                be one of "4", "6", "7", "2019-09", or "2020-12".
 *
 *                               If the |$schema| key is present in the |schema|, it
 *                               will be used to auto-detect the correct version.
 *                               Otherwise, 2019-09 will be used.
 * @param {boolean} options.shortCircuit  Whether or not the validator should
 *                                        return after a single error occurs.
 *
 * @returns {object} An object with |valid| and |errors| keys that indicates the
 *                   success of validation.
 */
function validate(
  instance,
  schema,
  { draft = detectSchemaDraft(schema), shortCircuit = true } = {}
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

function detectSchemaDraft(schema, defaultDraft = "2019-09") {
  const { $schema } = schema;

  if (typeof $schema === "undefined") {
    return defaultDraft;
  }

  switch ($schema) {
    case "http://json-schema.org/draft-04/schema#":
      return "4";

    case "http://json-schema.org/draft-06/schema#":
      return "6";

    case "http://json-schema.org/draft-07/schema#":
      return "7";

    case "https://json-schema.org/draft/2019-09/schema":
      return "2019-09";

    case "https://json-schema.org/draft/2020-12/schema":
      return "2020-12";

    default:
      Cu.reportError(
        `Unexpected $schema "${$schema}", defaulting to ${defaultDraft}.`
      );
      return defaultDraft;
  }
}

const JsonSchema = {
  Validator,
  validate,
  detectSchemaDraft,
};

const EXPORTED_SYMBOLS = ["JsonSchema"];
