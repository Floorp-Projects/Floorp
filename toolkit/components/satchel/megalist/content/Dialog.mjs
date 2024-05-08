/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const GENERIC_DIALOG_TEMPLATE = document.querySelector("#dialog-template");

const DIALOGS = {
  "remove-login": {
    template: "#remove-login-dialog-template",
  },
  "export-logins": {
    template: "#export-logins-dialog-template",
  },
  "remove-logins": {
    template: "#remove-logins-dialog-template",
    callback: dialog => {
      const primaryButton = dialog.querySelector("button.primary");
      const checkbox = dialog.querySelector(".confirm-checkbox");
      const toggleButton = () => (primaryButton.disabled = !checkbox.checked);
      checkbox.addEventListener("change", toggleButton);
      toggleButton();
    },
  },
  "import-logins": {
    template: "#import-logins-dialog-template",
  },
  "import-error": {
    template: "#import-error-dialog-template",
  },
};

/**
 * Setup dismiss and command handling logic for the dialog overlay.
 *
 * @param {Element} overlay - The overlay element containing the dialog
 * @param {Function} messageHandler - Function to send message back to view model.
 */
const setupControls = (overlay, messageHandler) => {
  const dialog = overlay.querySelector(".dialog-container");
  const commandButtons = dialog.querySelectorAll("[data-command]");
  for (const commandButton of commandButtons) {
    const commandId = commandButton.dataset.command;
    commandButton.addEventListener("click", () => messageHandler(commandId));
  }

  dialog.querySelectorAll("[close-dialog]").forEach(element => {
    element.addEventListener("click", cancelDialog, { once: true });
  });

  document.addEventListener("keyup", function handleKeyUp(ev) {
    if (ev.key === "Escape") {
      cancelDialog();
      document.removeEventListener("keyup", handleKeyUp);
    }
  });

  document.addEventListener("click", function handleClickOutside(ev) {
    if (!dialog.contains(ev.target)) {
      cancelDialog();
      document.removeEventListener("click", handleClickOutside);
    }
  });
  dialog.querySelector("[autofocus]")?.focus();
};

/**
 * Add data-l10n-args to elements with localizable attribute
 *
 * @param {Element} dialog - The dialog element.
 * @param {Array<object>} l10nArgs - List of localization arguments.
 */
const populateL10nArgs = (dialog, l10nArgs) => {
  const localizableElements = dialog.querySelectorAll("[localizable]");
  for (const [index, localizableElement] of localizableElements.entries()) {
    localizableElement.dataset.l10nArgs = JSON.stringify(l10nArgs[index]) ?? "";
  }
};

/**
 * Remove the currently displayed dialog overlay from the DOM.
 */
export const cancelDialog = () =>
  document.querySelector(".dialog-overlay")?.remove();

/**
 * Create a new dialog overlay and populate it using the specified template and data.
 *
 * @param {object} dialogData - Data required to populate the dialog, includes template and localization args.
 * @param {Function} messageHandler - Function to send message back to view model.
 */
export const createDialog = (dialogData, messageHandler) => {
  const templateData = DIALOGS[dialogData?.id];

  const genericTemplateClone = document.importNode(
    GENERIC_DIALOG_TEMPLATE.content,
    true
  );

  const overlay = genericTemplateClone.querySelector(".dialog-overlay");
  const dialog = genericTemplateClone.querySelector(".dialog-container");

  const overrideTemplate = document.querySelector(templateData.template);
  const overrideTemplateClone = document.importNode(
    overrideTemplate.content,
    true
  );

  genericTemplateClone
    .querySelector(".dialog-wrapper")
    .appendChild(overrideTemplateClone);

  populateL10nArgs(genericTemplateClone, dialogData.l10nArgs);
  setupControls(overlay, messageHandler);
  document.body.appendChild(genericTemplateClone);
  templateData?.callback?.(dialog, messageHandler);
};
