/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { BaseAction } = ChromeUtils.import(
  "resource://normandy/actions/BaseAction.jsm"
);
const lazy = {};
ChromeUtils.defineModuleGetter(
  lazy,
  "ActionSchemas",
  "resource://normandy/actions/schemas/index.js"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "BrowserWindowTracker",
  "resource:///modules/BrowserWindowTracker.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "ClientEnvironment",
  "resource://normandy/lib/ClientEnvironment.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "Heartbeat",
  "resource://normandy/lib/Heartbeat.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "ShellService",
  "resource:///modules/ShellService.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "Storage",
  "resource://normandy/lib/Storage.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "NormandyUtils",
  "resource://normandy/lib/NormandyUtils.jsm"
);

var EXPORTED_SYMBOLS = ["ShowHeartbeatAction"];

XPCOMUtils.defineLazyGetter(lazy, "gAllRecipeStorage", function() {
  return new lazy.Storage("normandy-heartbeat");
});

const DAY_IN_MS = 24 * 60 * 60 * 1000;
const HEARTBEAT_THROTTLE = 1 * DAY_IN_MS;

class ShowHeartbeatAction extends BaseAction {
  static Heartbeat = lazy.Heartbeat;

  static overrideHeartbeatForTests(newHeartbeat) {
    if (newHeartbeat) {
      this.Heartbeat = newHeartbeat;
    } else {
      this.Heartbeat = lazy.Heartbeat;
    }
  }

  get schema() {
    return lazy.ActionSchemas["show-heartbeat"];
  }

  async _run(recipe) {
    const {
      message,
      engagementButtonLabel,
      thanksMessage,
      learnMoreMessage,
      learnMoreUrl,
    } = recipe.arguments;

    const recipeStorage = new lazy.Storage(recipe.id);

    if (!(await this.shouldShow(recipeStorage, recipe))) {
      return;
    }

    this.log.debug(
      `Heartbeat for recipe ${recipe.id} showing prompt "${message}"`
    );
    const targetWindow = lazy.BrowserWindowTracker.getTopWindow();

    if (!targetWindow) {
      throw new Error("No window to show heartbeat in");
    }

    const heartbeat = new ShowHeartbeatAction.Heartbeat(targetWindow, {
      surveyId: this.generateSurveyId(recipe),
      message,
      engagementButtonLabel,
      thanksMessage,
      learnMoreMessage,
      learnMoreUrl,
      postAnswerUrl: await this.generatePostAnswerURL(recipe),
      flowId: lazy.NormandyUtils.generateUuid(),
      surveyVersion: recipe.revision_id,
    });

    heartbeat.eventEmitter.once(
      "Voted",
      this.updateLastInteraction.bind(this, recipeStorage)
    );
    heartbeat.eventEmitter.once(
      "Engaged",
      this.updateLastInteraction.bind(this, recipeStorage)
    );

    let now = Date.now();
    await Promise.all([
      lazy.gAllRecipeStorage.setItem("lastShown", now),
      recipeStorage.setItem("lastShown", now),
    ]);
  }

  async shouldShow(recipeStorage, recipe) {
    const { repeatOption, repeatEvery } = recipe.arguments;
    // Don't show any heartbeats to a user more than once per throttle period
    let lastShown = await lazy.gAllRecipeStorage.getItem("lastShown");
    if (lastShown) {
      const duration = new Date() - lastShown;
      if (duration < HEARTBEAT_THROTTLE) {
        // show the number of hours since the last heartbeat, with at most 1 decimal point.
        const hoursAgo = Math.floor(duration / 1000 / 60 / 6) / 10;
        this.log.debug(
          `A heartbeat was shown too recently (${hoursAgo} hours), skipping recipe ${recipe.id}.`
        );
        return false;
      }
    }

    switch (repeatOption) {
      case "once": {
        // Don't show if we've ever shown before
        if (await recipeStorage.getItem("lastShown")) {
          this.log.debug(
            `Heartbeat for "once" recipe ${recipe.id} has been shown before, skipping.`
          );
          return false;
        }
        break;
      }

      case "nag": {
        // Show a heartbeat again only if the user has not interacted with it before
        if (await recipeStorage.getItem("lastInteraction")) {
          this.log.debug(
            `Heartbeat for "nag" recipe ${recipe.id} has already been interacted with, skipping.`
          );
          return false;
        }
        break;
      }

      case "xdays": {
        // Show this heartbeat again if it  has been at least `repeatEvery` days since the last time it was shown.
        let lastShown = await lazy.gAllRecipeStorage.getItem("lastShown");
        if (lastShown) {
          lastShown = new Date(lastShown);
          const duration = new Date() - lastShown;
          if (duration < repeatEvery * DAY_IN_MS) {
            // show the number of hours since the last time this hearbeat was shown, with at most 1 decimal point.
            const hoursAgo = Math.floor(duration / 1000 / 60 / 6) / 10;
            this.log.debug(
              `Heartbeat for "xdays" recipe ${recipe.id} ran in the last ${repeatEvery} days, skipping. (${hoursAgo} hours ago)`
            );
            return false;
          }
        }
      }
    }

    return true;
  }

  /**
   * Returns a surveyId value. If recipe calls to include the Normandy client
   * ID, then the client ID is attached to the surveyId in the format
   * `${surveyId}::${userId}`.
   *
   * @return {String} Survey ID, possibly with user UUID
   */
  generateSurveyId(recipe) {
    const { includeTelemetryUUID, surveyId } = recipe.arguments;
    if (includeTelemetryUUID) {
      return `${surveyId}::${lazy.ClientEnvironment.userId}`;
    }
    return surveyId;
  }

  /**
   * Generate the appropriate post-answer URL for a recipe.
   * @param  recipe
   * @return {String} URL with post-answer query params
   */
  async generatePostAnswerURL(recipe) {
    const { postAnswerUrl, message, includeTelemetryUUID } = recipe.arguments;

    // Don`t bother with empty URLs.
    if (!postAnswerUrl) {
      return postAnswerUrl;
    }

    const userId = lazy.ClientEnvironment.userId;
    const searchEngine = (await Services.search.getDefault()).identifier;
    const args = {
      fxVersion: Services.appinfo.version,
      isDefaultBrowser: lazy.ShellService.isDefaultBrowser() ? 1 : 0,
      searchEngine,
      source: "heartbeat",
      // `surveyversion` used to be the version of the heartbeat action when it
      // was hosted on a server. Keeping it around for compatibility.
      surveyversion: Services.appinfo.version,
      syncSetup: Services.prefs.prefHasUserValue("services.sync.username")
        ? 1
        : 0,
      updateChannel: lazy.UpdateUtils.getUpdateChannel(false),
      utm_campaign: encodeURIComponent(message.replace(/\s+/g, "")),
      utm_medium: recipe.action,
      utm_source: "firefox",
    };
    if (includeTelemetryUUID) {
      args.userId = userId;
    }

    let url = new URL(postAnswerUrl);
    // create a URL object to append arguments to
    for (const [key, val] of Object.entries(args)) {
      if (!url.searchParams.has(key)) {
        url.searchParams.set(key, val);
      }
    }

    // return the address with encoded queries
    return url.toString();
  }

  updateLastInteraction(recipeStorage) {
    recipeStorage.setItem("lastInteraction", Date.now());
  }
}
