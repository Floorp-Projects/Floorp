/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Accessible states used to check node's state from the accessiblity API
 * perspective.
 *
 * Note: if gecko is built with --disable-accessibility, the interfaces
 * are not defined. This is why we use getters instead to be able to use
 * these statically.
 */

this.AccessibilityUtils = (function() {
  const FORCE_DISABLE_ACCESSIBILITY_PREF = "accessibility.force_disabled";

  // Accessible states.
  const {
    STATE_FOCUSABLE,
    STATE_INVISIBLE,
    STATE_LINKED,
    STATE_UNAVAILABLE,
  } = Ci.nsIAccessibleStates;

  // Accessible action for showing long description.
  const CLICK_ACTION = "click";

  // Roles that are considered focusable with the keyboard.
  const KEYBOARD_FOCUSABLE_ROLES = new Set([
    Ci.nsIAccessibleRole.ROLE_BUTTONMENU,
    Ci.nsIAccessibleRole.ROLE_CHECKBUTTON,
    Ci.nsIAccessibleRole.ROLE_COMBOBOX,
    Ci.nsIAccessibleRole.ROLE_EDITCOMBOBOX,
    Ci.nsIAccessibleRole.ROLE_ENTRY,
    Ci.nsIAccessibleRole.ROLE_LINK,
    Ci.nsIAccessibleRole.ROLE_LISTBOX,
    Ci.nsIAccessibleRole.ROLE_PASSWORD_TEXT,
    Ci.nsIAccessibleRole.ROLE_PUSHBUTTON,
    Ci.nsIAccessibleRole.ROLE_RADIOBUTTON,
    Ci.nsIAccessibleRole.ROLE_SLIDER,
    Ci.nsIAccessibleRole.ROLE_SPINBUTTON,
    Ci.nsIAccessibleRole.ROLE_SUMMARY,
    Ci.nsIAccessibleRole.ROLE_SWITCH,
    Ci.nsIAccessibleRole.ROLE_TOGGLE_BUTTON,
  ]);

  // Roles that are user interactive.
  const INTERACTIVE_ROLES = new Set([
    ...KEYBOARD_FOCUSABLE_ROLES,
    Ci.nsIAccessibleRole.ROLE_CHECK_MENU_ITEM,
    Ci.nsIAccessibleRole.ROLE_CHECK_RICH_OPTION,
    Ci.nsIAccessibleRole.ROLE_COMBOBOX_OPTION,
    Ci.nsIAccessibleRole.ROLE_MENUITEM,
    Ci.nsIAccessibleRole.ROLE_OPTION,
    Ci.nsIAccessibleRole.ROLE_OUTLINE,
    Ci.nsIAccessibleRole.ROLE_OUTLINEITEM,
    Ci.nsIAccessibleRole.ROLE_PAGETAB,
    Ci.nsIAccessibleRole.ROLE_PARENT_MENUITEM,
    Ci.nsIAccessibleRole.ROLE_RADIO_MENU_ITEM,
    Ci.nsIAccessibleRole.ROLE_RICH_OPTION,
  ]);

  // Roles that are considered interactive when they are focusable.
  const INTERACTIVE_IF_FOCUSABLE_ROLES = new Set([
    // If article is focusable, we can assume it is inside a feed.
    Ci.nsIAccessibleRole.ROLE_ARTICLE,
    // Column header can be focusable.
    Ci.nsIAccessibleRole.ROLE_COLUMNHEADER,
    Ci.nsIAccessibleRole.ROLE_GRID_CELL,
    Ci.nsIAccessibleRole.ROLE_MENUBAR,
    Ci.nsIAccessibleRole.ROLE_MENUPOPUP,
    Ci.nsIAccessibleRole.ROLE_PAGETABLIST,
    // Row header can be focusable.
    Ci.nsIAccessibleRole.ROLE_ROWHEADER,
    Ci.nsIAccessibleRole.ROLE_SCROLLBAR,
    Ci.nsIAccessibleRole.ROLE_SEPARATOR,
    Ci.nsIAccessibleRole.ROLE_TOOLBAR,
  ]);

  // Roles that are considered form controls.
  const FORM_ROLES = new Set([
    Ci.nsIAccessibleRole.ROLE_CHECKBUTTON,
    Ci.nsIAccessibleRole.ROLE_CHECK_RICH_OPTION,
    Ci.nsIAccessibleRole.ROLE_COMBOBOX,
    Ci.nsIAccessibleRole.ROLE_EDITCOMBOBOX,
    Ci.nsIAccessibleRole.ROLE_ENTRY,
    Ci.nsIAccessibleRole.ROLE_LISTBOX,
    Ci.nsIAccessibleRole.ROLE_PASSWORD_TEXT,
    Ci.nsIAccessibleRole.ROLE_PROGRESSBAR,
    Ci.nsIAccessibleRole.ROLE_RADIOBUTTON,
    Ci.nsIAccessibleRole.ROLE_SLIDER,
    Ci.nsIAccessibleRole.ROLE_SPINBUTTON,
    Ci.nsIAccessibleRole.ROLE_SWITCH,
  ]);

  const DEFAULT_ENV = Object.freeze({
    // Checks that accessible object has at least one accessible action.
    actionCountRule: true,
    // Checks that accessible object (and its corresponding node) is focusable
    // (has focusable state and its node's tabindex is not set to -1).
    focusableRule: true,
    // Checks that clickable accessible object (and its corresponding node) has
    // appropriate interactive semantics.
    ifClickableThenInteractiveRule: true,
    // Checks that accessible object has a role that is considered to be
    // interactive.
    interactiveRule: true,
    // Checks that accessible object has a non-empty label.
    labelRule: true,
    // Checks that a node has a corresponging accessible object.
    mustHaveAccessibleRule: true,
    // Checks that accessible object (and its corresponding node) have a non-
    // negative tabindex. Platform accessibility API still sets focusable state
    // on tabindex=-1 nodes.
    nonNegativeTabIndexRule: true,
  });

  let gA11YChecks = false;

  let gEnv = {
    ...DEFAULT_ENV,
  };

  /**
   * Get role attribute for an accessible object if specified for its
   * corresponding {@code DOMNode}.
   *
   * @param   {nsIAccessible} accessible
   *          Accessible for which to determine its role attribute value.
   *
   * @returns {String}
   *          Role attribute value if specified.
   */
  function getAriaRoles(accessible) {
    try {
      return accessible.attributes.getStringProperty("xml-roles");
    } catch (e) {
      // No xml-roles. nsPersistentProperties throws if the attribute for a key
      // is not found.
    }

    return "";
  }

  /**
   * Get related accessible objects that are targets of labelled by relation e.g.
   * labels.
   * @param   {nsIAccessible} accessible
   *          Accessible objects to get labels for.
   *
   * @returns {Array}
   *          A list of accessible objects that are labels for a given accessible.
   */
  function getLabels(accessible) {
    const relation = accessible.getRelationByType(
      Ci.nsIAccessibleRelation.RELATION_LABELLED_BY
    );
    return [...relation.getTargets().enumerate(Ci.nsIAccessible)];
  }

  /**
   * Test if an accessible has a {@code hidden} attribute.
   *
   * @param  {nsIAccessible} accessible
   *         Accessible object.
   *
   * @return {boolean}
   *         True if the accessible object has a {@code hidden} attribute, false
   *         otherwise.
   */
  function hasHiddenAttribute(accessible) {
    let hidden = false;
    try {
      hidden = accessible.attributes.getStringProperty("hidden");
    } catch (e) {}
    // If the property is missing, error will be thrown
    return hidden && hidden === "true";
  }

  /**
   * Test if an accessible is hidden from the user.
   *
   * @param  {nsIAccessible} accessible
   *         Accessible object.
   *
   * @return {boolean}
   *         True if accessible is hidden from user, false otherwise.
   */
  function isHidden(accessible) {
    if (!accessible) {
      return true;
    }

    while (accessible) {
      if (hasHiddenAttribute(accessible)) {
        return true;
      }

      accessible = accessible.parent;
    }

    return false;
  }

  /**
   * Check if an accessible has a given state.
   *
   * @param  {nsIAccessible} accessible
   *         Accessible object to test.
   * @param  {number} stateToMatch
   *         State to match.
   *
   * @return {boolean}
   *         True if |accessible| has |stateToMatch|, false otherwise.
   */
  function matchState(accessible, stateToMatch) {
    const state = {};
    accessible.getState(state, {});

    return !!(state.value & stateToMatch);
  }

  /**
   * Determine if accessible is focusable with the keyboard.
   *
   * @param   {nsIAccessible} accessible
   *          Accessible for which to determine if it is keyboard focusable.
   *
   * @returns {Boolean}
   *          True if focusable with the keyboard.
   */
  function isKeyboardFocusable(accessible) {
    // State will be focusable even if the tabindex is negative.
    return (
      matchState(accessible, STATE_FOCUSABLE) &&
      // Platform accessibility will still report STATE_FOCUSABLE even with the
      // tabindex="-1" so we need to check that it is >= 0 to be considered
      // keyboard focusable.
      (!gEnv.nonNegativeTabIndexRule || accessible.DOMNode.tabIndex > -1)
    );
  }

  function buildMessage(message, DOMNode) {
    if (DOMNode) {
      const { id, tagName, className } = DOMNode;
      message += `: id: ${id}, tagName: ${tagName}, className: ${className}`;
    }

    return message;
  }

  /**
   * Fail a test with a given message because of an issue with a given
   * accessible object. This is used for cases where there's an actual
   * accessibility failure that prevents UI from being accessible to keyboard/AT
   * users.
   *
   * @param {String} message
   * @param {nsIAccessible} accessible
   *        Accessible to log along with the failure message.
   */
  function a11yFail(message, { DOMNode }) {
    SpecialPowers.SimpleTest.ok(false, buildMessage(message, DOMNode));
  }

  /**
   * Log a todo statement with a given message because of an issue with a given
   * accessible object. This is used for cases where accessibility best
   * practices are not followed or for something that is not as severe to be
   * considered a failure.
   * @param {String} message
   * @param {nsIAccessible} accessible
   *        Accessible to log along with the todo message.
   */
  function a11yWarn(message, { DOMNode }) {
    SpecialPowers.SimpleTest.todo(false, buildMessage(message, DOMNode));
  }

  /**
   * Test if the node's unavailable via the accessibility API.
   *
   * @param {nsIAccessible} accessible
   *        Accessible object.
   */
  function assertEnabled(accessible) {
    if (matchState(accessible, STATE_UNAVAILABLE)) {
      a11yFail(
        "Node is enabled but disabled via the accessibility API",
        accessible
      );
    }
  }

  /**
   * Test if it is possible to focus on a node with the keyboard. This method
   * also checks for additional keyboard focus issues that might arise.
   *
   * @param {nsIAccessible} accessible
   *        Accessible object for a node.
   */
  function assertFocusable(accessible) {
    if (gEnv.focusableRule && !isKeyboardFocusable(accessible)) {
      const ariaRoles = getAriaRoles(accessible);
      // Do not force ARIA combobox or listbox to be focusable.
      if (!ariaRoles.includes("combobox") && !ariaRoles.includes("listbox")) {
        a11yFail("Node is not focusable via the accessibility API", accessible);
      }

      return;
    }

    if (!INTERACTIVE_IF_FOCUSABLE_ROLES.has(accessible.role)) {
      // ROLE_TABLE is used for grids too which are considered interactive.
      if (
        accessible.role === Ci.nsIAccessibleRole.ROLE_TABLE &&
        !getAriaRoles(accessible).includes("grid")
      ) {
        a11yWarn(
          "Focusable nodes should have interactive semantics",
          accessible
        );

        return;
      }
    }

    if (accessible.DOMNode.tabIndex > 0) {
      a11yWarn("Avoid using tabindex attribute greater than zero", accessible);
    }
  }

  /**
   * Test if it is possible to interact with a node via the accessibility API.
   *
   * @param {nsIAccessible} accessible
   *        Accessible object for a node.
   */
  function assertInteractive(accessible) {
    if (gEnv.actionCountRule && accessible.actionCount === 0) {
      a11yFail("Node does not support any accessible actions", accessible);

      return;
    }

    if (gEnv.interactiveRule && !INTERACTIVE_ROLES.has(accessible.role)) {
      if (
        // Labels that have a label for relation with their target are clickable.
        (accessible.role !== Ci.nsIAccessibleRole.ROLE_LABEL ||
          accessible.getRelationByType(
            Ci.nsIAccessibleRelation.RELATION_LABEL_FOR
          ).targetsCount === 0) &&
        // Images that are inside an anchor (have linked state).
        (accessible.role !== Ci.nsIAccessibleRole.ROLE_GRAPHIC ||
          !matchState(accessible, STATE_LINKED))
      ) {
        // Look for click action in the list of actions.
        for (let i = 0; i < accessible.actionCount; i++) {
          if (
            gEnv.ifClickableThenInteractiveRule &&
            accessible.getActionName(i) === CLICK_ACTION
          ) {
            a11yFail(
              "Clickable nodes must have interactive semantics",
              accessible
            );
          }
        }
      }

      a11yFail(
        "Node does not have a correct interactive role and may not be " +
          "manipulated via the accessibility API",
        accessible
      );
    }
  }

  /**
   * Test if the node is labelled appropriately for accessibility API.
   *
   * @param {nsIAccessible} accessible
   *        Accessible object for a node.
   */
  function assertLabelled(accessible) {
    const name = accessible.name && accessible.name.trim();
    if (gEnv.labelRule && !name) {
      a11yFail("Interactive elements must be labeled", accessible);

      return;
    }

    const { DOMNode } = accessible;
    if (FORM_ROLES.has(accessible.role)) {
      const labels = getLabels(accessible);
      const hasNameFromVisibleLabel = labels.some(
        label => !matchState(label, STATE_INVISIBLE)
      );

      if (!hasNameFromVisibleLabel) {
        a11yWarn("Form elements should have a visible text label", accessible);
      }
    } else if (
      accessible.role === Ci.nsIAccessibleRole.ROLE_LINK &&
      DOMNode.nodeName === "AREA" &&
      DOMNode.hasAttribute("href")
    ) {
      const alt = DOMNode.getAttribute("alt");
      if (alt && alt.trim() !== name) {
        a11yFail(
          "Use alt attribute to label area elements that have the href attribute",
          accessible
        );
      }
    }
  }

  /**
   * Test if the node's visible via accessibility API.
   *
   * @param {nsIAccessible} accessible
   *        Accessible object for a node.
   */
  function assertVisible(accessible) {
    if (isHidden(accessible)) {
      a11yFail(
        "Node is not currently visible via the accessibility API and may not " +
          "be manipulated by it",
        accessible
      );
    }
  }

  /**
   * Walk node ancestry and force refresh driver tick in every document.
   * @param {DOMNode} node
   *        Node for traversing the ancestry.
   */
  function forceRefreshDriverTick(node) {
    const wins = [];
    let bc = BrowsingContext.getFromWindow(node.ownerDocument.defaultView); // eslint-disable-line
    while (bc) {
      wins.push(bc.associatedWindow);
      bc = bc.embedderWindowGlobal?.browsingContext;
    }

    let win = wins.pop();
    while (win) {
      // Stop the refresh driver from doing its regular ticks and force two
      // refresh driver ticks: first to let layout update and notify a11y,  and
      // the second to let a11y process updates.
      win.windowUtils.advanceTimeAndRefresh(100);
      win.windowUtils.advanceTimeAndRefresh(100);
      // Go back to normal refresh driver ticks.
      win.windowUtils.restoreNormalRefresh();
      win = wins.pop();
    }
  }

  /**
   * Get an accessible object for a node.
   * Note: this method will not resolve if accessible object does not become
   * available for a given node.
   *
   * @param  {DOMNode} node
   *         Node to get the accessible object for.
   *
   * @return {nsIAccessible}
   *         Accessibility object for a given node.
   */
  function getAccessible(node) {
    const accessibilityService = Cc[
      "@mozilla.org/accessibilityService;1"
    ].getService(Ci.nsIAccessibilityService);
    if (!accessibilityService) {
      // This is likely a build with --disable-accessibility
      return null;
    }

    let acc = accessibilityService.getAccessibleFor(node);
    if (acc) {
      return acc;
    }

    // Force refresh tick throughout document hierarchy
    forceRefreshDriverTick(node);
    return accessibilityService.getAccessibleFor(node);
  }

  function runIfA11YChecks(task) {
    return (...args) => (gA11YChecks ? task(...args) : null);
  }

  /**
   * AccessibilityUtils provides utility methods for retrieving accessible objects
   * and performing accessibility related checks.
   * Current methods:
   *   assertCanBeClicked
   *   setEnv
   *   resetEnv
   *
   */
  const AccessibilityUtils = {
    assertCanBeClicked(node) {
      const acc = getAccessible(node);
      if (!acc) {
        if (gEnv.mustHaveAccessibleRule) {
          a11yFail("Node is not accessible via accessibility API", {
            DOMNode: node,
          });
        }

        return;
      }

      assertInteractive(acc);
      assertFocusable(acc);
      assertVisible(acc);
      assertEnabled(acc);
      assertLabelled(acc);
    },

    setEnv(env = DEFAULT_ENV) {
      gEnv = {
        ...DEFAULT_ENV,
        ...env,
      };
    },

    resetEnv() {
      gEnv = { ...DEFAULT_ENV };
    },

    reset(a11yChecks = false) {
      gA11YChecks = a11yChecks;

      const { Services } = SpecialPowers;
      // Disable accessibility service if it is running and if a11y checks are
      // disabled.
      if (!gA11YChecks && Services.appinfo.accessibilityEnabled) {
        Services.prefs.setIntPref(FORCE_DISABLE_ACCESSIBILITY_PREF, 1);
        Services.prefs.clearUserPref(FORCE_DISABLE_ACCESSIBILITY_PREF);
      }

      // Reset accessibility environment flags that might've been set within the
      // test.
      this.resetEnv();
    },
  };

  AccessibilityUtils.assertCanBeClicked = runIfA11YChecks(
    AccessibilityUtils.assertCanBeClicked.bind(AccessibilityUtils)
  );

  AccessibilityUtils.setEnv = runIfA11YChecks(
    AccessibilityUtils.setEnv.bind(AccessibilityUtils)
  );

  AccessibilityUtils.resetEnv = runIfA11YChecks(
    AccessibilityUtils.resetEnv.bind(AccessibilityUtils)
  );

  return AccessibilityUtils;
})();
