/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { JsonSchema } = ChromeUtils.importESModule(
  "resource://gre/modules/JsonSchema.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  SpecialMessageActions:
    "resource://messaging-system/lib/SpecialMessageActions.sys.mjs",
});

ChromeUtils.defineLazyGetter(this, "fetchSMASchema", async () => {
  const response = await fetch(
    "resource://testing-common/SpecialMessageActionSchemas.json"
  );
  const schema = await response.json();
  if (!schema) {
    throw new Error("Failed to load SpecialMessageActionSchemas");
  }
  return schema;
});

const EXAMPLE_URL = "https://example.com/";

const SMATestUtils = {
  /**
   * Checks if an action is valid acording to existing schemas
   * @param {SpecialMessageAction} action
   */
  async validateAction(action) {
    const schema = await fetchSMASchema;
    const result = JsonSchema.validate(action, schema);
    if (result.errors.length) {
      throw new Error(
        `Action with type ${
          action.type
        } was not valid. Errors: ${JSON.stringify(result.errors, undefined, 2)}`
      );
    }
    is(
      result.errors.length,
      0,
      `Should be a valid action of type ${action.type}`
    );
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
