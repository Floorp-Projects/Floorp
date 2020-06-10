/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "SpecialMessageActions",
  "resource://messaging-system/lib/SpecialMessageActions.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "JsonSchemaValidator",
  "resource://gre/modules/components-utils/JsonSchemaValidator.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "SpecialMessageActionSchemas",
  "resource://testing-common/SpecialMessageActionSchemas.js"
);

const EXAMPLE_URL = "https://example.com/";

const SMATestUtils = {
  /**
   * Checks if an action is valid acording to existing schemas
   * @param {SpecialMessageAction} action
   */
  async validateAction(action) {
    const schema = SpecialMessageActionSchemas[action.type];
    ok(schema, `should have a schema for ${action.type}`);
    const { valid, error } = JsonSchemaValidator.validate(action, schema);
    if (!valid) {
      throw new Error(
        `Action with type ${action.type} was not valid: ${error.message}`
      );
    }
    ok(valid, `should be a valid action of type ${action.type}`);
  },

  /**
   * Executes a Special Message Action after validating it
   * @param {SpecialMessageAction} action
   * @param {Browser} browser
   */
  async executeAndValidateAction(action, browser = gBrowser) {
    SMATestUtils.validateAction(action);
    await SpecialMessageActions.handleAction(action, browser);
  },
};
