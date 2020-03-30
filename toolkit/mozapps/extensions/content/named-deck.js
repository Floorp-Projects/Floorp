/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint max-len: ["error", 80] */

"use strict";

/**
 * This element is for use with the <named-deck> element. Set the target
 * <named-deck>'s ID in the "deck" attribute and the button's selected state
 * will reflect the deck's state. When the button is clicked, it will set the
 * view in the <named-deck> to the button's "name" attribute.
 *
 * The "tab" role will be added unless a different role is provided. Wrapping
 * a set of these buttons in a <button-group> element will add the key handling
 * for a tablist.
 *
 * NOTE: This does not observe changes to the "deck" or "name" attributes, so
 * changing them likely won't work properly.
 *
 * <button is="named-deck-button" deck="pet-deck" name="dogs">Dogs</button>
 * <named-deck id="pet-deck">
 *   <p name="cats">I like cats.</p>
 *   <p name="dogs">I like dogs.</p>
 * </named-deck>
 *
 * let btn = document.querySelector('button[name="dogs"]');
 * let deck = document.querySelector("named-deck");
 * deck.selectedViewName == "cats";
 * btn.selected == false; // Selected was pulled from the related deck.
 * btn.click();
 * deck.selectedViewName == "dogs";
 * btn.selected == true; // Selected updated when view changed.
 */
class NamedDeckButton extends HTMLButtonElement {
  connectedCallback() {
    this.id = `${this.deckId}-button-${this.name}`;
    if (!this.hasAttribute("role")) {
      this.setAttribute("role", "tab");
    }
    this.setSelectedFromDeck();
    this.addEventListener("click", this);
    document.addEventListener("view-changed", this, { capture: true });
  }

  disconnectedCallback() {
    this.removeEventListener("click", this);
    document.removeEventListener("view-changed", this, { capture: true });
  }

  attributeChangedCallback(name, oldVal, newVal) {
    if (name == "selected") {
      this.selected = newVal;
    }
  }

  get deckId() {
    return this.getAttribute("deck");
  }

  set deckId(val) {
    this.setAttribute("deck", val);
  }

  get deck() {
    return document.getElementById(this.deckId);
  }

  handleEvent(e) {
    if (e.type == "view-changed" && e.target.id == this.deckId) {
      this.setSelectedFromDeck();
    } else if (e.type == "click") {
      let { deck } = this;
      if (deck) {
        deck.selectedViewName = this.name;
      }
    }
  }

  get name() {
    return this.getAttribute("name");
  }

  get selected() {
    return this.hasAttribute("selected");
  }

  set selected(val) {
    if (this.selected != val) {
      this.toggleAttribute("selected", val);
    }
    this.setAttribute("aria-selected", !!val);
  }

  setSelectedFromDeck() {
    let { deck } = this;
    this.selected = deck && deck.selectedViewName == this.name;
    if (this.selected) {
      this.dispatchEvent(
        new CustomEvent("button-group:selected", { bubbles: true })
      );
    }
  }
}
customElements.define("named-deck-button", NamedDeckButton, {
  extends: "button",
});

class ButtonGroup extends HTMLElement {
  static get observedAttributes() {
    return ["orientation"];
  }

  connectedCallback() {
    this.setAttribute("role", "tablist");

    if (!this.observer) {
      this.observer = new MutationObserver(changes => {
        for (let change of changes) {
          this.setChildAttributes(change.addedNodes);
          for (let node of change.removedNodes) {
            if (this.activeChild == node) {
              // Ensure there's still an active child.
              this.activeChild = this.firstElementChild;
            }
          }
        }
      });
    }
    this.observer.observe(this, { childList: true });

    // Set the role and tabindex for the current children.
    this.setChildAttributes(this.children);

    // Try assigning the active child again, this will run through the checks
    // to ensure it's still valid.
    this.activeChild = this._activeChild;

    this.addEventListener("button-group:selected", this);
    this.addEventListener("keydown", this);
  }

  disconnectedCallback() {
    this.observer.disconnect();
    this.removeEventListener("button-group:selected", this);
    this.removeEventListener("keydown", this);
  }

  attributeChangedCallback(name, oldVal, newVal) {
    if (name == "orientation") {
      if (this.isVertical) {
        this.setAttribute("aria-orientation", this.orientation);
      } else {
        this.removeAttribute("aria-orientation");
      }
    }
  }

  setChildAttributes(nodes) {
    for (let node of nodes) {
      if (node.nodeType == Node.ELEMENT_NODE && node != this.activeChild) {
        node.setAttribute("tabindex", "-1");
      }
    }
  }

  // The activeChild is the child that can be focused with tab.
  get activeChild() {
    return this._activeChild;
  }

  set activeChild(node) {
    let prevActiveChild = this._activeChild;
    let newActiveChild;

    if (node && this.contains(node)) {
      newActiveChild = node;
    } else {
      newActiveChild = this.firstElementChild;
    }

    this._activeChild = newActiveChild;

    if (newActiveChild) {
      newActiveChild.setAttribute("tabindex", "0");
    }

    if (prevActiveChild && prevActiveChild != newActiveChild) {
      prevActiveChild.setAttribute("tabindex", "-1");
    }
  }

  get isVertical() {
    return this.orientation == "vertical";
  }

  get orientation() {
    return this.getAttribute("orientation") == "vertical"
      ? "vertical"
      : "horizontal";
  }

  set orientation(val) {
    if (val == "vertical") {
      this.setAttribute("orientation", val);
    } else {
      this.removeAttribute("orientation");
    }
  }

  _navigationKeys() {
    if (this.isVertical) {
      return {
        previousKey: "ArrowUp",
        nextKey: "ArrowDown",
      };
    }
    if (document.dir == "rtl") {
      return {
        previousKey: "ArrowRight",
        nextKey: "ArrowLeft",
      };
    }
    return {
      previousKey: "ArrowLeft",
      nextKey: "ArrowRight",
    };
  }

  handleEvent(e) {
    let { previousKey, nextKey } = this._navigationKeys();
    if (e.type == "keydown" && (e.key == previousKey || e.key == nextKey)) {
      e.preventDefault();
      let oldFocus = this.activeChild;
      this.walker.currentNode = oldFocus;
      let newFocus;
      if (e.key == previousKey) {
        newFocus = this.walker.previousNode();
      } else {
        newFocus = this.walker.nextNode();
      }
      if (newFocus) {
        this.activeChild = newFocus;
      }
    } else if (e.type == "button-group:selected") {
      this.activeChild = e.target;
    }
  }

  get walker() {
    if (!this._walker) {
      this._walker = document.createTreeWalker(this, NodeFilter.SHOW_ELEMENT, {
        acceptNode: node => {
          if (node.hidden || node.disabled) {
            return NodeFilter.FILTER_REJECT;
          }
          node.focus();
          return document.activeElement == node
            ? NodeFilter.FILTER_ACCEPT
            : NodeFilter.FILTER_REJECT;
        },
      });
    }
    return this._walker;
  }
}
customElements.define("button-group", ButtonGroup);

/**
 * A deck that is indexed by the "name" attribute of its children. The
 * <named-deck-button> element is a companion element that can update its state
 * and change the view of a <named-deck>.
 *
 * When the deck is connected it will set the first child as the selected view
 * if a view is not already selected.
 *
 * The deck is implemented using a named slot. Setting a slot directly on a
 * child element of the deck is not supported.
 *
 * You can get or set the selected view by name with the `selectedViewName`
 * property or by setting the "selected-view" attribute.
 *
 * <named-deck>
 *   <section name="cats">Some info about cats.</section>
 *   <section name="dogs">Some dog stuff.</section>
 * </named-deck>
 *
 * let deck = document.querySelector("named-deck");
 * deck.selectedViewName == "cats"; // Cat info is shown.
 * deck.selectedViewName = "dogs";
 * deck.selectedViewName == "dogs"; // Dog stuff is shown.
 * deck.setAttribute("selected-view", "cats");
 * deck.selectedViewName == "cats"; // Cat info is shown.
 */
class NamedDeck extends HTMLElement {
  static get observedAttributes() {
    return ["selected-view"];
  }

  constructor() {
    super();
    this.attachShadow({ mode: "open" });

    // Create a slot for the visible content.
    let selectedSlot = document.createElement("slot");
    selectedSlot.setAttribute("name", "selected");
    this.shadowRoot.appendChild(selectedSlot);

    this.observer = new MutationObserver(() => {
      this._setSelectedViewAttributes();
    });
  }

  connectedCallback() {
    if (this.selectedViewName) {
      // Make sure the selected view is shown.
      this._setSelectedViewAttributes();
    } else {
      // If there's no selected view, default to the first.
      let firstView = this.firstElementChild;
      if (firstView) {
        // This will trigger showing the first view.
        this.selectedViewName = firstView.getAttribute("name");
      }
    }
    this.observer.observe(this, { childList: true });
  }

  disconnectedCallback() {
    this.observer.disconnect();
  }

  attributeChangedCallback(attr, oldVal, newVal) {
    if (attr == "selected-view" && oldVal != newVal) {
      // Update the slot attribute on the views.
      this._setSelectedViewAttributes();

      // Notify that the selected view changed.
      this.dispatchEvent(new CustomEvent("view-changed"));
    }
  }

  get selectedViewName() {
    return this.getAttribute("selected-view");
  }

  set selectedViewName(name) {
    this.setAttribute("selected-view", name);
  }

  /**
   * Set the slot attribute on all of the views to ensure only the selected view
   * is shown.
   */
  _setSelectedViewAttributes() {
    let { selectedViewName } = this;
    for (let view of this.children) {
      let name = view.getAttribute("name");
      view.setAttribute("aria-labelledby", `${this.id}-button-${name}`);
      view.setAttribute("role", "tabpanel");

      if (name === selectedViewName) {
        view.slot = "selected";
      } else {
        view.slot = "";
      }
    }
  }
}
customElements.define("named-deck", NamedDeck);
