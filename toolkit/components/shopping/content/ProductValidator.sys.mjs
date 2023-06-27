/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { JsonSchema } from "resource://gre/modules/JsonSchema.sys.mjs";

let schemas = {};

/**
 * Validate JSON result from the shopping API.
 *
 * @param {object} json
 *  JSON object from the API request.
 * @returns {boolean} result
 *  If the JSON is valid or not.
 */
async function validate(json, SchemaURL) {
  if (!schemas[SchemaURL]) {
    schemas[SchemaURL] = await fetch(SchemaURL).then(rsp => rsp.json());
  }

  let result = JsonSchema.validate(json, schemas[SchemaURL]);

  // TODO: maybe just for debug?
  if (!result.valid) {
    console.error(
      `Invalid result: ${JSON.stringify(result.errors, undefined, 2)}`
    );
  }
  return result.valid;
}

export const ProductValidator = {
  validate,
};
