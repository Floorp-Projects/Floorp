/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { BranchedAddonStudyAction } = ChromeUtils.import(
  "resource://normandy/actions/BranchedAddonStudyAction.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ActionSchemas: "resource://normandy/actions/schemas/index.js",
  AddonStudies: "resource://normandy/lib/AddonStudies.jsm",
});

var EXPORTED_SYMBOLS = ["AddonStudyAction"];

/*
 * This action was originally the only form of add-on studies. Later, a version
 * of add-on studies was addded that supported having more than one
 * experimental branch per study, instead of relying on the installed add-on to
 * manage its branches. To reduce duplicated code, the no-branches version of
 * the action inherits from the multi-branch version.
 *
 * The schemas of the arguments for these two actions are different. As well as
 * supporting branches within the study, the multi-branch version also changed
 * its metadata fields to better match the use cases of studies.
 *
 * This action translates a legacy no branches study into a single branched
 * study with the proper metadata. This should be considered a temporary
 * measure, and eventually all studies will be native multi-branch studies.
 *
 * The existing schema can't be changed, because these legacy recipes are also
 * sent by the server to older clients that don't support the newer schema
 * format.
 */

class AddonStudyAction extends BranchedAddonStudyAction {
  get schema() {
    return ActionSchemas["addon-study"];
  }

  /**
   * This hook is executed once for each recipe that currently applies to this
   * client. It is responsible for:
   *
   *   - Translating recipes to match BranchedAddonStudy's schema
   *   - Validating that transformation
   *   - Calling BranchedAddonStudy's _run hook.
   *
   * If the recipe fails to enroll or update, it should throw to properly
   * report its status.
   */
  async _run(recipe) {
    const args = recipe.arguments; // save some typing

    /*
     * The argument schema of no-branches add-ons don't include a separate slug
     * and name, and use different names for the description. Convert from the
     * old to the new one.
     */
    let transformedArguments = {
      slug: args.name,
      userFacingName: args.name,
      userFacingDescription: args.description,
      isEnrollmentPaused: !!args.isEnrollmentPaused,
      branches: [
        {
          slug: AddonStudies.NO_BRANCHES_MARKER,
          ratio: 1,
          extensionApiId: recipe.arguments.extensionApiId,
        },
      ],
    };

    // This will throw if the arguments aren't valid, and BaseAction will catch it.
    transformedArguments = this.validateArguments(
      transformedArguments,
      ActionSchemas["branched-addon-study"]
    );

    const transformedRecipe = { ...recipe, arguments: transformedArguments };
    return super._run(transformedRecipe);
  }

  /**
   * This hook is executed once after all recipes that apply to this client
   * have been processed. It is responsible for unenrolling the client from any
   * studies that no longer apply, based on this.seenRecipeIds, which is set by
   * the super class.
   */
  async _finalize() {
    const activeStudies = await AddonStudies.getAllActive({
      branched: AddonStudies.FILTER_NOT_BRANCHED,
    });

    for (const study of activeStudies) {
      if (!this.seenRecipeIds.has(study.recipeId)) {
        this.log.debug(
          `Stopping non-branched add-on study for recipe ${study.recipeId}`
        );
        try {
          await this.unenroll(study.recipeId, "recipe-not-seen");
        } catch (err) {
          Cu.reportError(err);
        }
      }
    }
  }
}
