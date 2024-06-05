/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/**
 * Class implementing the nsICredentialChooserService.
 *
 * This class shows UI to the user for the Credential Chooser for the
 * Credential Management API.
 *
 * @class CredentialChooserService
 */
export class CredentialChooserService {
  classID = Components.ID("{673ddc19-03e2-4b30-a868-06297e8fed89}");
  QueryInterface = ChromeUtils.generateQI(["nsICredentialChooserService"]);

  /**
   * This private member holds the set of choices made by tests via
   * testMakeChoices. If an entry exists while running showCredentialChooser,
   * we choose the credential with id in the map or none.
   */
  #testChoiceIds = new WeakMap();

  /**
   * This function displays the credential chooser UI, allowing the user to make an identity choice.
   * Once the user makes a choice from the credentials provided, or dismisses the prompt, we will
   * call the callback with that credential, or null in the case of a dismiss.
   *
   * We also support UI-less testing via choices provided by testMakeChoice. If there is a choice in
   * memory, we use that as an immediate choice of any credential with that id, or none.
   *
   * @param {BrowsingContext} browsingContext The top browsing context of the window calling the Credential Management API
   * @param {Array<Credential>} credentials The credentials the user should choose from
   * @param {nsICredentialChosenCallback} callback A callback to return the user's credential choice to
   * @returns {nsresult}
   */
  showCredentialChooser(browsingContext, credentials, callback) {
    let testChoice = this.#testChoiceIds.get(browsingContext);
    if (testChoice) {
      let credential = credentials.find(cred => cred.id == testChoice);
      if (!credential) {
        callback.notify(null);
      } else {
        callback.notify(credential);
      }
      return Cr.NS_OK;
    }

    // We do not have credential chooser UI yet. To be added in Bug 1892021.
    callback.notify(null);
    return Cr.NS_ERROR_NOT_IMPLEMENTED;
  }

  /**
   * Dismiss the credential chooser dialog for this browsing context's window.
   *
   * @param {BrowsingContext} _browsingContext The top browsing context of the window calling the Credential Management API
   */
  cancelCredentialChooser(_browsingContext) {}

  /**
   * A test helper that allows us to make decisions about which credentials to use without showing any UI.
   * This must be called before showCredentialChooser with the same browsing context.
   *
   * @param {BrowsingContext} browsingContext The top browsing context of the window calling the Credential Management API
   * @param {UTF8String} id The id to be used in place of user choice when showCredentialChooser is called
   */
  testMakeChoice(browsingContext, id) {
    this.#testChoiceIds.set(browsingContext, id);
  }
}
