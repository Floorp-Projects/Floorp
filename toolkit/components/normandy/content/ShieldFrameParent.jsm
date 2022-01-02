/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["ShieldFrameParent"];

const frameGlobal = {};
ChromeUtils.defineModuleGetter(
  frameGlobal,
  "AboutPages",
  "resource://normandy-content/AboutPages.jsm"
);

class ShieldFrameParent extends JSWindowActorParent {
  async receiveMessage(msg) {
    let { aboutStudies } = frameGlobal.AboutPages;
    switch (msg.name) {
      case "Shield:AddToWeakSet":
        aboutStudies.addToWeakSet(this.browsingContext);
        break;
      case "Shield:RemoveFromWeakSet":
        aboutStudies.removeFromWeakSet(this.browsingContext);
        break;
      case "Shield:GetAddonStudyList":
        return aboutStudies.getAddonStudyList();
      case "Shield:GetPreferenceStudyList":
        return aboutStudies.getPreferenceStudyList();
      case "Shield:GetMessagingSystemList":
        return aboutStudies.getMessagingSystemList();
      case "Shield:RemoveAddonStudy":
        aboutStudies.removeAddonStudy(msg.data.recipeId, msg.data.reason);
        break;
      case "Shield:RemovePreferenceStudy":
        aboutStudies.removePreferenceStudy(
          msg.data.experimentName,
          msg.data.reason
        );
        break;
      case "Shield:RemoveMessagingSystemExperiment":
        aboutStudies.removeMessagingSystemExperiment(
          msg.data.slug,
          msg.data.reason
        );
        break;
      case "Shield:OpenDataPreferences":
        aboutStudies.openDataPreferences();
        break;
      case "Shield:GetStudiesEnabled":
        return aboutStudies.getStudiesEnabled();
      case "Shield:ExperimentOptIn":
        return aboutStudies.optInToExperiment(msg.data);
    }

    return null;
  }
}
