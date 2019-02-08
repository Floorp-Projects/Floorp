/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* exported initialize, hide, show */

"use strict";

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
});

const PLUGIN_ICON_URL = "chrome://global/skin/plugins/pluginGeneric.svg";

let _templates = {};

/**
 * Import a template from the main document.
 */
function importTemplate(name) {
  if (!_templates.hasOwnProperty(name)) {
    _templates[name] = document.querySelector(`template[name="${name}"]`);
  }
  let template = _templates[name];
  if (template) {
    return document.importNode(template.content, true);
  }
  throw new Error(`Unknown template: ${name}`);
}

class ListView {
  constructor({param, root}) {
    this.type = param;
    this.root = root;
  }

  async getAddons() {
    let addons = await AddonManager.getAddonsByTypes([this.type]);
    addons = addons.filter(addon => !addon.isSystem);

    // Sort by name.
    addons.sort((a, b) => a.name.localeCompare(b.name));
    return addons;
  }

  setEnableLabel(button, disabled) {
    if (disabled) {
      document.l10n.setAttributes(button, "enable-addon-button");
    } else {
      document.l10n.setAttributes(button, "disable-addon-button");
    }
  }

  updateCard(card, addon) {
    let icon;
    if (addon.type == "plugin") {
      icon = PLUGIN_ICON_URL;
    } else {
      icon = AddonManager.getPreferredIconURL(addon, 32, window);
    }
    card.querySelector(".addon-icon").src = icon;
    card.querySelector(".addon-name").textContent = addon.name;
    card.querySelector(".addon-description").textContent = addon.description;

    this.setEnableLabel(
      card.querySelector('[action="toggle-disabled"]'), addon.userDisabled);
  }

  renderAddonCard(addon) {
    let card = importTemplate("card").firstElementChild;
    card.setAttribute("addon-id", addon.id);

    // Set the contents.
    this.updateCard(card, addon);

    card.addEventListener("click", async (e) => {
      switch (e.target.getAttribute("action")) {
        case "toggle-disabled":
          if (addon.userDisabled) {
            await addon.enable();
          } else {
            await addon.disable();
          }
          this.render();
          break;
        case "remove":
          await addon.uninstall();
          this.render();
          break;
      }
    });
    return card;
  }

  renderSections({disabledFrag, enabledFrag}) {
    let viewFrag = importTemplate("list");
    let enabledSection = viewFrag.querySelector('[type="enabled"]');
    let disabledSection = viewFrag.querySelector('[type="disabled"]');

    // Set the cards or remove the section if empty.
    let setSection = (section, cards) => {
      if (cards.children.length > 0) {
        section.appendChild(cards);
      } else {
        section.remove();
      }
    };

    setSection(enabledSection, enabledFrag);
    setSection(disabledSection, disabledFrag);

    return viewFrag;
  }

  async render() {
    let addons = await this.getAddons();

    let disabledFrag = document.createDocumentFragment();
    let enabledFrag = document.createDocumentFragment();

    // Populate fragments for the enabled and disabled sections.
    for (let addon of addons) {
      let card = this.renderAddonCard(addon);
      if (addon.userDisabled) {
        disabledFrag.appendChild(card);
      } else {
        enabledFrag.appendChild(card);
      }
    }

    // Put the sections into the main template.
    let frag = this.renderSections({disabledFrag, enabledFrag});

    this.root.textContent = "";
    this.root.appendChild(frag);
  }
}

// Generic view management.
let root = null;

/**
 * Called from extensions.js once, when about:addons is loading.
 */
function initialize() {
  root = document.getElementById("main");
}

/**
 * Called from extensions.js to load a view. The view's render method should
 * resolve once the view has been updated to conform with other about:addons
 * views.
 */
async function show(type, param) {
  if (type == "list") {
    await new ListView({param, root}).render();
  }
}

function hide() {
  root.textContent = "";
}
