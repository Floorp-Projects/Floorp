/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  let logger = lazy.LoginHelper.createLogger("LoginRelatedRealms");
  return logger;
});

export class LoginRelatedRealmsParent extends JSWindowActorParent {
  /**
   * @type RemoteSettingsClient
   *
   * @memberof LoginRelatedRealmsParent
   */
  _sharedCredentialsClient = null;
  /**
   * @type string[][]
   *
   * @memberof LoginRelatedRealmsParent
   */
  _relatedDomainsList = [[]];

  /**
   * Handles the Remote Settings sync event
   *
   * @param {Object} aEvent
   * @param {Array} aEvent.current Records that are currently in the collection after the sync event
   * @param {Array} aEvent.created Records that were created
   * @param {Array} aEvent.updated Records that were updated
   * @param {Array} aEvent.deleted Records that were deleted
   * @memberof LoginRelatedRealmsParent
   */
  onRemoteSettingsSync(aEvent) {
    let {
      data: { current },
    } = aEvent;
    this._relatedDomainsList = current;
  }

  async getSharedCredentialsCollection() {
    if (!this._sharedCredentialsClient) {
      this._sharedCredentialsClient = lazy.RemoteSettings(
        lazy.LoginHelper.relatedRealmsCollection
      );
      this._sharedCredentialsClient.on("sync", event =>
        this.onRemoteSettingsSync(event)
      );
      this._relatedDomainsList = await this._sharedCredentialsClient.get();
    }
    return this._relatedDomainsList;
  }

  /**
   * Determine if there are any related realms of this `formOrigin` using the related realms collection
   * @param {string} formOrigin A form origin
   * @return {string[]} filteredRealms An array of domains related to the `formOrigin`
   * @async
   * @memberof LoginRelatedRealmsParent
   */
  async findRelatedRealms(formOrigin) {
    try {
      let formOriginURI = Services.io.newURI(formOrigin);
      let originDomain = formOriginURI.host;
      let [{ relatedRealms } = {}] =
        await this.getSharedCredentialsCollection();
      if (!relatedRealms) {
        return [];
      }
      let filterOriginIndex;
      let shouldInclude = false;
      let filteredRealms = relatedRealms.filter(_realms => {
        for (let relatedOrigin of _realms) {
          // We can't have an origin that matches multiple entries in our related realms collection
          // so we exit the loop early
          if (shouldInclude) {
            return false;
          }
          if (Services.eTLD.hasRootDomain(originDomain, relatedOrigin)) {
            shouldInclude = true;
            break;
          }
        }
        return shouldInclude;
      });
      // * Filtered realms is a nested array due to its structure in Remote Settings
      filteredRealms = filteredRealms.flat();

      filterOriginIndex = filteredRealms.indexOf(originDomain);
      // Removing the current formOrigin match if it exists in the related realms
      // so that we don't return duplicates when we search for logins
      if (filterOriginIndex !== -1) {
        filteredRealms.splice(filterOriginIndex, 1);
      }
      return filteredRealms;
    } catch (e) {
      lazy.log.error(e);
      return [];
    }
  }
}
