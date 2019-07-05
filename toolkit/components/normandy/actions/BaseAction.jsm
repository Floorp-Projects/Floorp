/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.defineModuleGetter(
  this,
  "LogManager",
  "resource://normandy/lib/LogManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Uptake",
  "resource://normandy/lib/Uptake.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "JsonSchemaValidator",
  "resource://gre/modules/components-utils/JsonSchemaValidator.jsm"
);

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
    this.state = BaseAction.STATE_PREPARING;
    this.log = LogManager.getLogger(`action.${this.name}`);
    this.lastError = null;
  }

  /**
   * Be sure to run the _preExecution() hook once during its
   * lifecycle.
   *
   * This is not intended for overriding by subclasses.
   */
  _ensurePreExecution() {
    if (this.state !== BaseAction.STATE_PREPARING) {
      return;
    }

    try {
      this._preExecution();
      // if _preExecution changed the state, don't overwrite it
      if (this.state === BaseAction.STATE_PREPARING) {
        this.state = BaseAction.STATE_READY;
      }
    } catch (err) {
      // Sometimes err.message is editable. If it is, add helpful details.
      // Otherwise log the helpful details and move on.
      try {
        err.message = `Could not initialize action ${this.name}: ${
          err.message
        }`;
      } catch (_e) {
        this.log.error(
          `Could not initialize action ${this.name}, error follows.`
        );
      }
      this.fail(err);
    }
  }

  get schema() {
    return {
      type: "object",
      properties: {},
    };
  }

  /**
   * Disable the action for a non-error reason, such as the user opting out of
   * this type of action.
   */
  disable() {
    this.state = BaseAction.STATE_DISABLED;
  }

  fail(err) {
    switch (this.state) {
      case BaseAction.STATE_PREPARING: {
        Uptake.reportAction(this.name, Uptake.ACTION_PRE_EXECUTION_ERROR);
        break;
      }
      default: {
        Cu.reportError(
          new Error("BaseAction.fail() called at unexpected time")
        );
      }
    }
    this.state = BaseAction.STATE_FAILED;
    this.lastError = err;
    Cu.reportError(err);
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

  validateArguments(args, schema = this.schema) {
    let [valid, validated] = JsonSchemaValidator.validateAndParseParameters(
      args,
      schema
    );
    if (!valid) {
      throw new Error(
        `Arguments do not match schema. arguments:\n${JSON.stringify(args)}\n` +
          `schema:\n${JSON.stringify(schema)}`
      );
    }
    return validated;
  }

  /**
   * Execute the per-recipe behavior of this action for a given
   * recipe.  Reports Uptake telemetry for the execution of the recipe.
   *
   * @param {Recipe} recipe
   * @throws If this action has already been finalized.
   */
  async runRecipe(recipe) {
    this._ensurePreExecution();

    if (this.state === BaseAction.STATE_FINALIZED) {
      throw new Error("Action has already been finalized");
    }

    if (this.state !== BaseAction.STATE_READY) {
      Uptake.reportRecipe(recipe, Uptake.RECIPE_ACTION_DISABLED);
      this.log.warn(
        `Skipping recipe ${recipe.name} because ${
          this.name
        } was disabled during preExecution.`
      );
      return;
    }

    try {
      recipe.arguments = this.validateArguments(recipe.arguments);
    } catch (error) {
      Cu.reportError(error);
      Uptake.reportRecipe(recipe, Uptake.RECIPE_EXECUTION_ERROR);
      return;
    }

    let status = Uptake.RECIPE_SUCCESS;
    try {
      await this._run(recipe);
    } catch (err) {
      Cu.reportError(err);
      status = Uptake.RECIPE_EXECUTION_ERROR;
    }
    Uptake.reportRecipe(recipe, status);
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
    // It's possible that no recipes matched us, so runRecipe() was
    // never called. In that case, we should ensure that we call
    // _preExecute() here.
    this._ensurePreExecution();

    let status;
    switch (this.state) {
      case BaseAction.STATE_FINALIZED: {
        throw new Error("Action has already been finalized");
      }
      case BaseAction.STATE_READY: {
        try {
          await this._finalize();
          status = Uptake.ACTION_SUCCESS;
        } catch (err) {
          status = Uptake.ACTION_POST_EXECUTION_ERROR;
          // Sometimes Error.message can be updated in place. This gives better messages when debugging errors.
          try {
            err.message = `Could not run postExecution hook for ${this.name}: ${
              err.message
            }`;
          } catch (err) {
            // Sometimes Error.message cannot be updated. Log a warning, and move on.
            this.log.debug(`Could not run postExecution hook for ${this.name}`);
          }

          this.lastError = err;
          Cu.reportError(err);
        }
        break;
      }
      case BaseAction.STATE_DISABLED: {
        this.log.debug(
          `Skipping post-execution hook for ${
            this.name
          } because it is disabled.`
        );
        status = Uptake.ACTION_SUCCESS;
        break;
      }
      case BaseAction.STATE_FAILED: {
        this.log.debug(
          `Skipping post-execution hook for ${
            this.name
          } because it failed during pre-execution.`
        );
        // Don't report a status. A status should have already been reported by this.fail().
        break;
      }
      default: {
        throw new Error(`Unexpected state during finalize: ${this.state}`);
      }
    }

    this.state = BaseAction.STATE_FINALIZED;
    if (status) {
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

BaseAction.STATE_PREPARING = "ACTION_PREPARING";
BaseAction.STATE_READY = "ACTION_READY";
BaseAction.STATE_DISABLED = "ACTION_DISABLED";
BaseAction.STATE_FAILED = "ACTION_FAILED";
BaseAction.STATE_FINALIZED = "ACTION_FINALIZED";
