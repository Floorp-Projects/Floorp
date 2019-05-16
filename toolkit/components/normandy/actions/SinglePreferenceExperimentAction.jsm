/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {PreferenceExperimentAction} = ChromeUtils.import("resource://normandy/actions/PreferenceExperimentAction.jsm");
ChromeUtils.defineModuleGetter(this, "ActionSchemas", "resource://normandy/actions/schemas/index.js");
ChromeUtils.defineModuleGetter(this, "JsonSchemaValidator", "resource://gre/modules/components-utils/JsonSchemaValidator.jsm");

var EXPORTED_SYMBOLS = ["SinglePreferenceExperimentAction"];

/**
 * The backwards-compatible version of the preference experiment that
 * can only accept a single preference.
 */
class SinglePreferenceExperimentAction extends PreferenceExperimentAction {
  get schema() {
    return ActionSchemas["single-preference-experiment"];
  }

  async _run(recipe) {
    const {
      preferenceBranchType,
      preferenceName,
      preferenceType,
      branches,
      ...remainingArguments
    } = recipe.arguments;

    const newArguments = {
      // The multiple-preference-experiment schema requires a string
      // name/description, which are necessary in the wire format, but
      // experiment objects can have null for these fields. Add some
      // filler fields here and remove them after validation.
      userFacingName: "temp-name",
      userFacingDescription: "temp-description",
      ...remainingArguments,
      branches: branches.map(branch => {
        const { value, ...branchProps } = branch;
        return {
          ...branchProps,
          preferences: {
            [preferenceName]: {
              preferenceBranchType,
              preferenceType,
              preferenceValue: value,
            },
          },
        };
      }),
    };

    const multiprefSchema = ActionSchemas["multiple-preference-experiment"];

    let [valid, validatedArguments] = JsonSchemaValidator.validateAndParseParameters(newArguments, multiprefSchema);
    if (!valid) {
      throw new Error(`Transformed arguments do not match schema. Original arguments: ${JSON.stringify(recipe.arguments)}, new arguments: ${JSON.stringify(newArguments)}, schema: ${JSON.stringify(multiprefSchema)}`);
    }

    validatedArguments.userFacingName = null;
    validatedArguments.userFacingDescription = null;

    recipe.arguments = validatedArguments;

    const newRecipe = {
      ...recipe,
      arguments: validatedArguments,
    };

    return super._run(newRecipe);
  }
}
