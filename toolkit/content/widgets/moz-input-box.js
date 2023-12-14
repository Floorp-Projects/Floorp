/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
  const cachedFragments = {
    get editMenuItems() {
      return `
      <menuitem data-l10n-id="text-action-undo" cmd="cmd_undo"></menuitem>
      <menuitem data-l10n-id="text-action-redo" cmd="cmd_redo"></menuitem>
      <menuseparator></menuseparator>
      <menuitem data-l10n-id="text-action-cut" cmd="cmd_cut"></menuitem>
      <menuitem data-l10n-id="text-action-copy" cmd="cmd_copy"></menuitem>
      <menuitem data-l10n-id="text-action-paste" cmd="cmd_paste"></menuitem>
      <menuitem data-l10n-id="text-action-delete" cmd="cmd_delete"></menuitem>
      <menuitem data-l10n-id="text-action-select-all" cmd="cmd_selectAll"></menuitem>
    `;
    },
    get normal() {
      delete this.normal;
      this.normal = MozXULElement.parseXULToFragment(
        `
      <menupopup class="textbox-contextmenu" showservicesmenu="true">
        ${this.editMenuItems}
      </menupopup>
    `
      );
      MozXULElement.insertFTLIfNeeded("toolkit/global/textActions.ftl");
      return this.normal;
    },
    get spellcheck() {
      delete this.spellcheck;
      this.spellcheck = MozXULElement.parseXULToFragment(
        `
      <menupopup class="textbox-contextmenu" showservicesmenu="true">
        <menuitem data-l10n-id="text-action-spell-no-suggestions" anonid="spell-no-suggestions" disabled="true"></menuitem>
        <menuitem data-l10n-id="text-action-spell-add-to-dictionary" anonid="spell-add-to-dictionary" oncommand="this.parentNode.parentNode.spellCheckerUI.addToDictionary();"></menuitem>
        <menuitem data-l10n-id="text-action-spell-undo-add-to-dictionary" anonid="spell-undo-add-to-dictionary" oncommand="this.parentNode.parentNode.spellCheckerUI.undoAddToDictionary();"></menuitem>
        <menuseparator anonid="spell-suggestions-separator"></menuseparator>
        ${this.editMenuItems}
        <menuseparator anonid="spell-check-separator"></menuseparator>
        <menuitem data-l10n-id="text-action-spell-check-toggle" type="checkbox" anonid="spell-check-enabled" oncommand="this.parentNode.parentNode.spellCheckerUI.toggleEnabled();"></menuitem>
        <menu data-l10n-id="text-action-spell-dictionaries" anonid="spell-dictionaries">
          <menupopup anonid="spell-dictionaries-menu" onpopupshowing="event.stopPropagation();" onpopuphiding="event.stopPropagation();"></menupopup>
        </menu>
      </menupopup>
    `
      );
      return this.spellcheck;
    },
  };

  class MozInputBox extends MozXULElement {
    static get observedAttributes() {
      return ["spellcheck"];
    }

    attributeChangedCallback(name, oldValue, newValue) {
      if (name === "spellcheck" && oldValue != newValue) {
        this._initUI();
      }
    }

    connectedCallback() {
      this._initUI();
    }

    _initUI() {
      this.spellcheck = this.hasAttribute("spellcheck");
      if (this.menupopup) {
        this.menupopup.remove();
      }

      this.setAttribute("context", "_child");
      this.appendChild(
        this.spellcheck
          ? cachedFragments.spellcheck.cloneNode(true)
          : cachedFragments.normal.cloneNode(true)
      );
      this.menupopup = this.querySelector(".textbox-contextmenu");

      this.menupopup.addEventListener("popupshowing", event => {
        let input = this._input;
        if (document.commandDispatcher.focusedElement != input) {
          input.focus();
        }
        this._doPopupItemEnabling(event);
      });

      if (this.spellcheck) {
        this.menupopup.addEventListener("popuphiding", event => {
          if (this.spellCheckerUI) {
            this.spellCheckerUI.clearSuggestionsFromMenu();
            this.spellCheckerUI.clearDictionaryListFromMenu();
          }
        });
      }

      this.menupopup.addEventListener("command", event => {
        var cmd = event.originalTarget.getAttribute("cmd");
        if (cmd) {
          this.doCommand(cmd);
          event.stopPropagation();
        }
      });
    }

    _doPopupItemEnablingSpell(event) {
      var spellui = this.spellCheckerUI;
      if (!spellui || !spellui.canSpellCheck) {
        this._setMenuItemVisibility("spell-no-suggestions", false);
        this._setMenuItemVisibility("spell-check-enabled", false);
        this._setMenuItemVisibility("spell-check-separator", false);
        this._setMenuItemVisibility("spell-add-to-dictionary", false);
        this._setMenuItemVisibility("spell-undo-add-to-dictionary", false);
        this._setMenuItemVisibility("spell-suggestions-separator", false);
        this._setMenuItemVisibility("spell-dictionaries", false);
        return;
      }

      spellui.initFromEvent(event.rangeParent, event.rangeOffset);

      var enabled = spellui.enabled;
      var showUndo = spellui.canSpellCheck && spellui.canUndo();

      var enabledCheckbox = this.getMenuItem("spell-check-enabled");
      enabledCheckbox.setAttribute("checked", enabled);

      var overMisspelling = spellui.overMisspelling;
      this._setMenuItemVisibility("spell-add-to-dictionary", overMisspelling);
      this._setMenuItemVisibility("spell-undo-add-to-dictionary", showUndo);
      this._setMenuItemVisibility(
        "spell-suggestions-separator",
        overMisspelling || showUndo
      );

      // suggestion list
      var suggestionsSeparator = this.getMenuItem("spell-no-suggestions");
      var numsug = spellui.addSuggestionsToMenuOnParent(
        event.target,
        suggestionsSeparator,
        5
      );
      this._setMenuItemVisibility(
        "spell-no-suggestions",
        overMisspelling && numsug == 0
      );

      // dictionary list
      var dictionariesMenu = this.getMenuItem("spell-dictionaries-menu");
      var numdicts = spellui.addDictionaryListToMenu(dictionariesMenu, null);
      this._setMenuItemVisibility(
        "spell-dictionaries",
        enabled && numdicts > 1
      );
    }

    _doPopupItemEnabling(event) {
      if (this.spellcheck) {
        this._doPopupItemEnablingSpell(event);
      }

      let popupNode = event.target;
      var children = popupNode.childNodes;
      for (var i = 0; i < children.length; i++) {
        var command = children[i].getAttribute("cmd");
        if (command) {
          var controller =
            document.commandDispatcher.getControllerForCommand(command);
          var enabled = controller.isCommandEnabled(command);
          if (enabled) {
            children[i].removeAttribute("disabled");
          } else {
            children[i].setAttribute("disabled", "true");
          }
        }
      }
    }

    get spellCheckerUI() {
      if (!this._spellCheckInitialized) {
        this._spellCheckInitialized = true;

        try {
          const { InlineSpellChecker } = ChromeUtils.importESModule(
            "resource://gre/modules/InlineSpellChecker.sys.mjs"
          );
          this.InlineSpellCheckerUI = new InlineSpellChecker(
            this._input.editor
          );
        } catch (ex) {}
      }

      return this.InlineSpellCheckerUI;
    }

    getMenuItem(anonid) {
      return this.querySelector(`[anonid="${anonid}"]`);
    }

    _setMenuItemVisibility(anonid, visible) {
      this.getMenuItem(anonid).hidden = !visible;
    }

    doCommand(command) {
      var controller =
        document.commandDispatcher.getControllerForCommand(command);
      controller.doCommand(command);
    }

    get _input() {
      return (
        this.getElementsByAttribute("anonid", "input")[0] ||
        this.querySelector(".textbox-input")
      );
    }
  }

  customElements.define("moz-input-box", MozInputBox);
}
