/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ContextualIdentityService:
    "resource://gre/modules/ContextualIdentityService.sys.mjs",

  ContextualIdentityListener:
    "chrome://remote/content/shared/listeners/ContextualIdentityListener.sys.mjs",
  generateUUID: "chrome://remote/content/shared/UUID.sys.mjs",
});

/**
 * A UserContextManager instance keeps track of all public user contexts and
 * maps their internal platform.
 *
 * This class is exported for test purposes. Otherwise the UserContextManager
 * singleton should be used.
 */
export class UserContextManagerClass {
  #contextualIdentityListener;
  #userContextIds;

  DEFAULT_CONTEXT_ID = "default";
  DEFAULT_INTERNAL_ID = 0;

  constructor() {
    // Map from internal ids (numbers) from the ContextualIdentityService to
    // opaque UUIDs (string).
    this.#userContextIds = new Map();

    // The default user context is always using 0 as internal user context id
    // and should be exposed as "default" instead of a randomly generated id.
    this.#userContextIds.set(this.DEFAULT_INTERNAL_ID, this.DEFAULT_CONTEXT_ID);

    // Register other (non-default) public contexts.
    lazy.ContextualIdentityService.getPublicIdentities().forEach(identity =>
      this.#registerIdentity(identity)
    );

    this.#contextualIdentityListener = new lazy.ContextualIdentityListener();
    this.#contextualIdentityListener.on("created", this.#onIdentityCreated);
    this.#contextualIdentityListener.on("deleted", this.#onIdentityDeleted);
    this.#contextualIdentityListener.startListening();
  }

  destroy() {
    this.#contextualIdentityListener.off("created", this.#onIdentityCreated);
    this.#contextualIdentityListener.off("deleted", this.#onIdentityDeleted);
    this.#contextualIdentityListener.destroy();

    this.#userContextIds = null;
  }

  /**
   * Creates a new user context.
   *
   * @param {string} prefix
   *     The prefix to use for the name of the user context.
   *
   * @returns {string}
   *     The user context id of the new user context.
   */
  createContext(prefix = "remote") {
    // Prepare the opaque id and name beforehand.
    const userContextId = lazy.generateUUID();
    const name = `${prefix}-${userContextId}`;

    // Create the user context.
    const identity = lazy.ContextualIdentityService.create(name);
    const internalId = identity.userContextId;

    // An id has been set already by the contextual-identity-created observer.
    // Override it with `userContextId` to match the container name.
    this.#userContextIds.set(internalId, userContextId);

    return userContextId;
  }

  /**
   * Retrieve the user context id corresponding to the provided internal id.
   *
   * @param {number} internalId
   *     The internal user context id.
   *
   * @returns {string|null}
   *     The corresponding user context id or null if the user context does not
   *     exist.
   */
  getIdByInternalId(internalId) {
    if (this.#userContextIds.has(internalId)) {
      return this.#userContextIds.get(internalId);
    }
    return null;
  }

  /**
   * Retrieve the internal id corresponding to the provided user
   * context id.
   *
   * @param {string} userContextId
   *     The user context id.
   *
   * @returns {number|null}
   *     The internal user context id or null if the user context does not
   *     exist.
   */
  getInternalIdById(userContextId) {
    for (const [internalId, id] of this.#userContextIds) {
      if (userContextId == id) {
        return internalId;
      }
    }
    return null;
  }

  /**
   * Returns an array of all known user context ids.
   *
   * @returns {Array<string>}
   *     The array of user context ids.
   */
  getUserContextIds() {
    return Array.from(this.#userContextIds.values());
  }

  /**
   * Checks if the provided user context id is known by this UserContextManager.
   *
   * @param {string} userContextId
   *     The id of the user context to check.
   */
  hasUserContextId(userContextId) {
    return this.getUserContextIds().includes(userContextId);
  }

  /**
   * Removes a user context and closes all related container tabs.
   *
   * @param {string} userContextId
   *     The id of the user context to remove.
   * @param {object=} options
   * @param {boolean=} options.closeContextTabs
   *     Pass true if the tabs owned by the user context should also be closed.
   *     Defaults to false.
   */
  removeUserContext(userContextId, options = {}) {
    const { closeContextTabs = false } = options;

    if (!this.hasUserContextId(userContextId)) {
      return;
    }

    const internalId = this.getInternalIdById(userContextId);
    if (closeContextTabs) {
      lazy.ContextualIdentityService.closeContainerTabs(internalId);
    }
    lazy.ContextualIdentityService.remove(internalId);
  }

  #onIdentityCreated = (eventName, data) => {
    this.#registerIdentity(data.identity);
  };

  #onIdentityDeleted = (eventName, data) => {
    this.#userContextIds.delete(data.identity.userContextId);
  };

  #registerIdentity(identity) {
    // Note: the id for identities created via UserContextManagerClass.createContext
    // are overridden in createContext.
    this.#userContextIds.set(identity.userContextId, lazy.generateUUID());
  }
}

// Expose a shared singleton.
export const UserContextManager = new UserContextManagerClass();
