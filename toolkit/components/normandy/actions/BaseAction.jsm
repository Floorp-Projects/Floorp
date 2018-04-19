/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.defineModuleGetter(this, "LogManager", "resource://normandy/lib/LogManager.jsm");
ChromeUtils.defineModuleGetter(this, "Uptake", "resource://normandy/lib/Uptake.jsm");
ChromeUtils.defineModuleGetter(this, "JsonSchemaValidator", "resource://gre/modules/components-utils/JsonSchemaValidator.jsm");

var EXPORTED_SYMBOLS = ["BaseAction"];

/**
 * Base class for local actions.
 *
 * This should be subclassed. Subclasses must implement _run() for
 * per-recipe behavior, and may implement _preExecution and _finalize
 * for actions to be taken once before and after recipes are run.
 *
 * Other methods should be overridden with care, to maintain the life
 * cycle events and error reporting implemented by this class.
 */
class BaseAction {
  constructor() {
    this.finalized = false;
    this.failed = false;
    this.log = LogManager.getLogger(`action.${this.name}`);

    try {
      this._preExecution();
    } catch (err) {
      this.failed = true;
      err.message = `Could not initialize action ${this.name}: ${err.message}`;
      Cu.reportError(err);
      Uptake.reportAction(this.name, Uptake.ACTION_PRE_EXECUTION_ERROR);
    }
  }

  get schema() {
    return {
      type: "object",
      properties: {},
    };
  }

  // Gets the name of the action. Does not necessarily match the
  // server slug for the action.
  get name() {
    return this.constructor.name;
  }

  /**
   * Action specific pre-execution behavior should be implemented
   * here. It will be called once per execution session.
   */
  _preExecution() {
    // Does nothing, may be overridden
  }

  /**
   * Execute the per-recipe behavior of this action for a given
   * recipe.  Reports Uptake telemetry for the execution of the recipe.
   *
   * @param {Recipe} recipe
   * @throws If this action has already been finalized.
   */
  async runRecipe(recipe) {
    if (this.finalized) {
      throw new Error("Action has already been finalized");
    }

    if (this.failed) {
      Uptake.reportRecipe(recipe.id, Uptake.RECIPE_ACTION_DISABLED);
      this.log.warn(`Skipping recipe ${recipe.name} because ${this.name} failed during preExecution.`);
      return;
    }

    let [valid, validatedArguments] = JsonSchemaValidator.validateAndParseParameters(recipe.arguments, this.schema);
    if (!valid) {
      Cu.reportError(new Error(`Arguments do not match schema. arguments: ${JSON.stringify(recipe.arguments)}. schema: ${JSON.stringify(this.schema)}`));
      Uptake.reportRecipe(recipe.id, Uptake.RECIPE_EXECUTION_ERROR);
      return;
    }

    recipe.arguments = validatedArguments;

    let status = Uptake.RECIPE_SUCCESS;
    try {
      await this._run(recipe);
    } catch (err) {
      Cu.reportError(err);
      status = Uptake.RECIPE_EXECUTION_ERROR;
    }
    Uptake.reportRecipe(recipe.id, status);
  }

  /**
   * Action specific recipe behavior must be implemented here. It
   * will be executed once for reach recipe, being passed the recipe
   * as a parameter.
   */
  async _run(recipe) {
    throw new Error("Not implemented");
  }

  /**
   * Finish an execution session. After this method is called, no
   * other methods may be called on this method, and all relevant
   * recipes will be assumed to have been seen.
   */
  async finalize() {
    if (this.finalized) {
      throw new Error("Action has already been finalized");
    }

    if (this.failed) {
      this.log.info(`Skipping post-execution hook for ${this.name} due to earlier failure.`);
      return;
    }

    let status = Uptake.ACTION_SUCCESS;
    try {
      await this._finalize();
    } catch (err) {
      status = Uptake.ACTION_POST_EXECUTION_ERROR;
      err.message = `Could not run postExecution hook for ${this.name}: ${err.message}`;
      Cu.reportError(err);
    } finally {
      this.finalized = true;
      Uptake.reportAction(this.name, status);
    }
  }

  /**
   * Action specific post-execution behavior should be implemented
   * here. It will be executed once after all recipes have been
   * processed.
   */
  async _finalize() {
    // Does nothing, may be overridden
  }
}
