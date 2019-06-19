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
 * NOTE: This does not observe changes to the "deck" or "name" attributes, so
 * changing them likely won't work properly.
 *
 * <named-deck-button deck="pet-deck" name="dogs">Dogs</named-deck-button>
 * <named-deck id="pet-deck">
 *   <p name="cats">I like cats.</p>
 *   <p name="dogs">I like dogs.</p>
 * </named-deck>
 *
 * let btn = document.querySelector("named-deck-button");
 * let deck = document.querySelector("named-deck");
 * deck.selectedViewName == "cats";
 * btn.selected == false; // Selected was pulled from the related deck.
 * btn.click();
 * deck.selectedViewName == "dogs";
 * btn.selected == true; // Selected updated when view changed.
 */
class NamedDeckButton extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({mode: "open"});
    // Include styles inline to avoid a FOUC.
    let style = document.createElement("style");
    style.textContent = `
      button {
        -moz-appearance: none;
        border: none;
        border-top: 2px solid transparent;
        border-bottom: 2px solid transparent;
        background: var(--in-content-box-background);
        font-size: 14px;
        line-height: 20px;
        padding: 4px 16px;
        color: var(--in-content-text-color);
      }

      button:hover {
        background-color: var(--in-content-box-background-hover);
        border-top-color: var(--in-content-box-border-color);
      }

      button:hover:active {
        background-color: var(--in-content-box-background-active);
      }

      :host([selected]) button {
        border-top-color: var(--in-content-border-highlight);
        color: var(--in-content-category-text-selected);
      }
    `;
    this.shadowRoot.appendChild(style);

    let button = document.createElement("button");
    button.appendChild(document.createElement("slot"));
    this.shadowRoot.appendChild(button);

    this.addEventListener("click", this);
  }

  connectedCallback() {
    this.setSelectedFromDeck();
    document.addEventListener("view-changed", this, {capture: true});
  }

  disconnectedCallback() {
    document.removeEventListener("view-changed", this, {capture: true});
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
      let {deck} = this;
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
    this.toggleAttribute("selected", !!val);
  }

  setSelectedFromDeck() {
    let {deck} = this;
    this.selected = deck && deck.selectedViewName == this.name;
  }
}
customElements.define("named-deck-button", NamedDeckButton);

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
    this.attachShadow({mode: "open"});

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
    this.observer.observe(this, {childList: true});
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
    let {selectedViewName} = this;
    for (let view of this.children) {
      if (view.getAttribute("name") == selectedViewName) {
        view.slot = "selected";
      } else {
        view.slot = "";
      }
    }
  }
}
customElements.define("named-deck", NamedDeck);
