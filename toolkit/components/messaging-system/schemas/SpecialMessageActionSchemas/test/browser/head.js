/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "SpecialMessageActions",
  "resource://messaging-system/lib/SpecialMessageActions.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "Ajv",
  "resource://testing-common/ajv-4.1.1.js"
);

XPCOMUtils.defineLazyGetter(this, "fetchSMASchema", async () => {
  const response = await fetch(
    "resource://testing-common/SpecialMessageActionSchemas.json"
  );
  const schema = await response.json();
  if (!schema) {
    throw new Error("Failed to load SpecialMessageActionSchemas");
  }
  return schema.definitions.SpecialMessageActionSchemas;
});

const EXAMPLE_URL = "https://example.com/";

const SMATestUtils = {
  /**
   * Checks if an action is valid acording to existing schemas
   * @param {SpecialMessageAction} action
   */
  async validateAction(action) {
    const schema = await fetchSMASchema;
    const ajv = new Ajv({ async: "co*" });
    const validator = ajv.compile(schema);
    if (!validator(action)) {
      throw new Error(`Action with type ${action.type} was not valid.`);
    }
    ok(!validator.errors, `should be a valid action of type ${action.type}`);
  },

  /**
   * Executes a Special Message Action after validating it
   * @param {SpecialMessageAction} action
   * @param {Browser} browser
   */
  async executeAndValidateAction(action, browser = gBrowser) {
    await SMATestUtils.validateAction(action);
    await SpecialMessageActions.handleAction(action, browser);
  },
};
