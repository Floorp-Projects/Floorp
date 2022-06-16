/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["LoginRecipesContent", "LoginRecipesParent"];

const REQUIRED_KEYS = ["hosts"];
const OPTIONAL_KEYS = [
  "description",
  "notPasswordSelector",
  "notUsernameSelector",
  "passwordSelector",
  "pathRegex",
  "usernameSelector",
  "schema",
  "id",
  "last_modified",
];
const SUPPORTED_KEYS = REQUIRED_KEYS.concat(OPTIONAL_KEYS);

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyGlobalGetters(lazy, ["fetch"]);

ChromeUtils.defineModuleGetter(
  lazy,
  "LoginHelper",
  "resource://gre/modules/LoginHelper.jsm"
);

XPCOMUtils.defineLazyGetter(lazy, "log", () =>
  lazy.LoginHelper.createLogger("LoginRecipes")
);

XPCOMUtils.defineLazyModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.js",
});

/**
 * Create an instance of the object to manage recipes in the parent process.
 * Consumers should wait until {@link initializationPromise} resolves before
 * calling methods on the object.
 *
 * @constructor
 * @param {String} [aOptions.defaults=null] the URI to load the recipes from.
 *                                          If it's null, nothing is loaded.
 *
 */
function LoginRecipesParent(aOptions = { defaults: null }) {
  if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
    throw new Error(
      "LoginRecipesParent should only be used from the main process"
    );
  }
  this._defaults = aOptions.defaults;
  this.reset();
}

LoginRecipesParent.prototype = {
  /**
   * Promise resolved with an instance of itself when the module is ready.
   *
   * @type {Promise}
   */
  initializationPromise: null,

  /**
   * @type {bool} Whether default recipes were loaded at construction time.
   */
  _defaults: null,

  /**
   * @type {Map} Map of hosts (including non-default port numbers) to Sets of recipes.
   *             e.g. "example.com:8080" => Set({...})
   */
  _recipesByHost: null,

  /**
   * @type {Object} Instance of Remote Settings client that has access to the
   *                "password-recipes" collection
   */
  _rsClient: null,

  /**
   * @param {Object} aRecipes an object containing recipes to load for use. The object
   *                          should be compatible with JSON (e.g. no RegExp).
   * @return {Promise} resolving when the recipes are loaded
   */
  load(aRecipes) {
    let recipeErrors = 0;
    for (let rawRecipe of aRecipes.siteRecipes) {
      try {
        rawRecipe.pathRegex = rawRecipe.pathRegex
          ? new RegExp(rawRecipe.pathRegex)
          : undefined;
        this.add(rawRecipe);
      } catch (ex) {
        recipeErrors++;
        lazy.log.error("Error loading recipe", rawRecipe, ex);
      }
    }
    if (recipeErrors) {
      return Promise.reject(`There were ${recipeErrors} recipe error(s)`);
    }
    return Promise.resolve();
  },

  /**
   * Reset the set of recipes to the ones from the time of construction.
   */
  reset() {
    lazy.log.debug("Resetting recipes with defaults:", this._defaults);
    this._recipesByHost = new Map();
    if (this._defaults) {
      let initPromise;
      /**
       * Both branches rely on a JSON dump of the Remote Settings collection, packaged both in Desktop and Android.
       * The «legacy» mode will read the dump directly from the packaged resources.
       * With Remote Settings, the dump is used to initialize the local database without network,
       * and the list of password recipes can be refreshed without restarting and without software update.
       */
      if (lazy.LoginHelper.remoteRecipesEnabled) {
        if (!this._rsClient) {
          this._rsClient = lazy.RemoteSettings(
            lazy.LoginHelper.remoteRecipesCollection
          );
          // Set up sync observer to update local recipes from Remote Settings recipes
          this._rsClient.on("sync", event => this.onRemoteSettingsSync(event));
        }
        initPromise = this._rsClient.get();
      } else if (this._defaults.startsWith("resource://")) {
        initPromise = lazy
          .fetch(this._defaults)
          .then(resp => resp.json())
          .then(({ data }) => data);
      } else {
        lazy.log.error(
          "Invalid recipe path found, setting empty recipes list!"
        );
        initPromise = new Promise(() => []);
      }
      this.initializationPromise = initPromise.then(async siteRecipes => {
        Services.ppmm.broadcastAsyncMessage("clearRecipeCache");
        await this.load({ siteRecipes });
        return this;
      });
    } else {
      this.initializationPromise = Promise.resolve(this);
    }
  },

  /**
   * Validate the recipe is sane and then add it to the set of recipes.
   *
   * @param {Object} recipe
   */
  add(recipe) {
    lazy.log.debug("Adding recipe:", recipe);
    let recipeKeys = Object.keys(recipe);
    let unknownKeys = recipeKeys.filter(key => !SUPPORTED_KEYS.includes(key));
    if (unknownKeys.length) {
      throw new Error(
        "The following recipe keys aren't supported: " + unknownKeys.join(", ")
      );
    }

    let missingRequiredKeys = REQUIRED_KEYS.filter(
      key => !recipeKeys.includes(key)
    );
    if (missingRequiredKeys.length) {
      throw new Error(
        "The following required recipe keys are missing: " +
          missingRequiredKeys.join(", ")
      );
    }

    if (!Array.isArray(recipe.hosts)) {
      throw new Error("'hosts' must be a array");
    }

    if (!recipe.hosts.length) {
      throw new Error("'hosts' must be a non-empty array");
    }

    if (recipe.pathRegex && recipe.pathRegex.constructor.name != "RegExp") {
      throw new Error("'pathRegex' must be a regular expression");
    }

    const OPTIONAL_STRING_PROPS = [
      "description",
      "passwordSelector",
      "usernameSelector",
    ];
    for (let prop of OPTIONAL_STRING_PROPS) {
      if (recipe[prop] && typeof recipe[prop] != "string") {
        throw new Error(`'${prop}' must be a string`);
      }
    }

    // Add the recipe to the map for each host
    for (let host of recipe.hosts) {
      if (!this._recipesByHost.has(host)) {
        this._recipesByHost.set(host, new Set());
      }
      this._recipesByHost.get(host).add(recipe);
    }
  },

  /**
   * Currently only exact host matches are returned but this will eventually handle parent domains.
   *
   * @param {String} aHost (e.g. example.com:8080 [non-default port] or sub.example.com)
   * @return {Set} of recipes that apply to the host ordered by host priority
   */
  getRecipesForHost(aHost) {
    let hostRecipes = this._recipesByHost.get(aHost);
    if (!hostRecipes) {
      return new Set();
    }

    return hostRecipes;
  },

  /**
   * Handles the Remote Settings sync event for the "password-recipes" collection.
   *
   * @param {Object} aEvent
   * @param {Array} event.current Records in the "password-recipes" collection after the sync event
   * @param {Array} event.created Records that were created with this particular sync
   * @param {Array} event.updated Records that were updated with this particular sync
   * @param {Array} event.deleted Records that were deleted with this particular sync
   */
  onRemoteSettingsSync(aEvent) {
    this._recipesByHost = new Map();
    let {
      data: { current },
    } = aEvent;
    let recipes = {
      siteRecipes: current,
    };
    Services.ppmm.broadcastAsyncMessage("clearRecipeCache");
    this.load(recipes);
  },
};

const LoginRecipesContent = {
  _recipeCache: new WeakMap(),

  _clearRecipeCache() {
    lazy.log.debug("_clearRecipeCache");
    this._recipeCache = new WeakMap();
  },

  /**
   * Locally caches recipes for a given host.
   *
   * @param {String} aHost (e.g. example.com:8080 [non-default port] or sub.example.com)
   * @param {Object} win - the window of the host
   * @param {Set} recipes - recipes that apply to the host
   */
  cacheRecipes(aHost, win, recipes) {
    lazy.log.debug("cacheRecipes: for:", aHost);
    let recipeMap = this._recipeCache.get(win);

    if (!recipeMap) {
      recipeMap = new Map();
      this._recipeCache.set(win, recipeMap);
    }

    recipeMap.set(aHost, recipes);
  },

  /**
   * Tries to fetch recipes for a given host, using a local cache if possible.
   * Otherwise, the recipes are cached for later use.
   *
   * @param {String} aHost (e.g. example.com:8080 [non-default port] or sub.example.com)
   * @param {Object} win - the window of the host
   * @return {Set} of recipes that apply to the host
   */
  getRecipes(aHost, win) {
    let recipes;
    const recipeMap = this._recipeCache.get(win);

    if (recipeMap) {
      recipes = recipeMap.get(aHost);

      if (recipes) {
        return recipes;
      }
    }

    if (!Cu.isInAutomation) {
      // this is a blocking call we expect in tests and rarely expect in
      // production, for example when Remote Settings are updated.
      lazy.log.warn(
        "getRecipes: falling back to a synchronous message for:",
        aHost
      );
    }
    recipes = Services.cpmm.sendSyncMessage("PasswordManager:findRecipes", {
      formOrigin: aHost,
    })[0];
    this.cacheRecipes(aHost, win, recipes);

    return recipes;
  },

  /**
   * @param {Set} aRecipes - Possible recipes that could apply to the form
   * @param {FormLike} aForm - We use a form instead of just a URL so we can later apply
   * tests to the page contents.
   * @return {Set} a subset of recipes that apply to the form with the order preserved
   */
  _filterRecipesForForm(aRecipes, aForm) {
    let formDocURL = aForm.ownerDocument.location;
    let hostRecipes = aRecipes;
    let recipes = new Set();
    lazy.log.debug("_filterRecipesForForm", aRecipes);
    if (!hostRecipes) {
      return recipes;
    }

    for (let hostRecipe of hostRecipes) {
      if (
        hostRecipe.pathRegex &&
        !hostRecipe.pathRegex.test(formDocURL.pathname)
      ) {
        continue;
      }
      recipes.add(hostRecipe);
    }

    return recipes;
  },

  /**
   * Given a set of recipes that apply to the host, choose the one most applicable for
   * overriding login fields in the form.
   *
   * @param {Set} aRecipes The set of recipes to consider for the form
   * @param {FormLike} aForm The form where login fields exist.
   * @return {Object} The recipe that is most applicable for the form.
   */
  getFieldOverrides(aRecipes, aForm) {
    let recipes = this._filterRecipesForForm(aRecipes, aForm);
    lazy.log.debug(
      "getFieldOverrides: filtered recipes:",
      recipes.size,
      recipes
    );
    if (!recipes.size) {
      return null;
    }

    let chosenRecipe = null;
    // Find the first (most-specific recipe that involves field overrides).
    for (let recipe of recipes) {
      if (
        !recipe.usernameSelector &&
        !recipe.passwordSelector &&
        !recipe.notUsernameSelector &&
        !recipe.notPasswordSelector
      ) {
        continue;
      }

      chosenRecipe = recipe;
      break;
    }

    return chosenRecipe;
  },

  /**
   * @param {HTMLElement} aParent the element to query for the selector from.
   * @param {CSSSelector} aSelector the CSS selector to query for the login field.
   * @return {HTMLElement|null}
   */
  queryLoginField(aParent, aSelector) {
    if (!aSelector) {
      return null;
    }
    let field = aParent.ownerDocument.querySelector(aSelector);
    if (!field) {
      lazy.log.debug("Login field selector wasn't matched:", aSelector);
      return null;
    }
    // ownerGlobal doesn't exist in content privileged windows.
    if (
      // eslint-disable-next-line mozilla/use-ownerGlobal
      !aParent.ownerDocument.defaultView.HTMLInputElement.isInstance(field)
    ) {
      lazy.log.warn("Login field isn't an <input> so ignoring it:", aSelector);
      return null;
    }
    return field;
  },
};
