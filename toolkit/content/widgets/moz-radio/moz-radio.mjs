/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "../vendor/lit.all.mjs";
import { MozLitElement } from "../lit-utils.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-label.mjs";

const NAVIGATION_FORWARD = "forward";
const NAVIGATION_BACKWARD = "backward";

const NAVIGATION_VALUE = {
  [NAVIGATION_FORWARD]: 1,
  [NAVIGATION_BACKWARD]: -1,
};

const DIRECTION_RIGHT = "Right";
const DIRECTION_LEFT = "Left";

const NAVIGATION_DIRECTIONS = {
  LTR: {
    FORWARD: DIRECTION_RIGHT,
    BACKWARD: DIRECTION_LEFT,
  },
  RTL: {
    FORWARD: DIRECTION_LEFT,
    BACKWARD: DIRECTION_RIGHT,
  },
};

/**
 * Element used to group and associate moz-radio buttons so that they function
 * as a single form-control element.
 *
 * @tagname moz-radio-group
 * @property {string} label - Label for the group of moz-radio elements.
 * @property {string} name
 *  Input name of the radio group. Propagates to moz-radio children.
 * @property {string} value
 *  Selected value for the group. Changing the value updates the checked
 *  state of moz-radio children and vice versa.
 * @slot default - The radio group's content, intended for moz-radio elements.
 */
export class MozRadioGroup extends MozLitElement {
  #radioButtons = [];
  #value;

  static properties = {
    label: { type: String },
    name: { type: String },
  };

  static queries = {
    defaultSlot: "slot:not([name])",
    legend: "legend",
  };

  set value(newValue) {
    this.#value = newValue;
    let focusableIndex = this.focusableIndex;
    this.#radioButtons.forEach((button, index) => {
      button.checked = this.value === button.value;
      button.inputTabIndex = focusableIndex === index ? 0 : -1;
    });
  }

  get value() {
    return this.#value;
  }

  get focusableIndex() {
    if (!this.#value) {
      return 0;
    }
    return this.#radioButtons.findIndex(button => button.value === this.#value);
  }

  constructor() {
    super();
    this.addEventListener("keydown", e => this.handleKeydown(e));
  }

  connectedCallback() {
    super.connectedCallback();
    this.dataset.l10nAttrs = "label";
  }

  firstUpdated() {
    this.syncStateToRadioButtons();
  }

  async getUpdateComplete() {
    await super.getUpdateComplete();
    await Promise.all(this.#radioButtons.map(button => button.updateComplete));
  }

  syncStateToRadioButtons() {
    this.#radioButtons = this.defaultSlot
      ?.assignedElements()
      .filter(el => el.localName === "moz-radio");

    let focusableIndex = this.focusableIndex;
    this.#radioButtons.forEach((button, index) => {
      if (button.checked && this.value == undefined) {
        this.value = button.value;
      }
      button.name = this.name;
      button.inputTabIndex = focusableIndex === index ? 0 : -1;
    });
  }

  // NB: We may need to revise this to avoid bugs when we add more focusable
  // elements to moz-radio-group / moz-radio.
  handleKeydown(event) {
    let directions = this.getNavigationDirections();
    switch (event.key) {
      case "Down":
      case "ArrowDown":
      case directions.FORWARD:
      case `Arrow${directions.FORWARD}`: {
        event.preventDefault();
        this.navigate(NAVIGATION_FORWARD);
        break;
      }
      case "Up":
      case "ArrowUp":
      case directions.BACKWARD:
      case `Arrow${directions.BACKWARD}`: {
        event.preventDefault();
        this.navigate(NAVIGATION_BACKWARD);
        break;
      }
    }
  }

  getNavigationDirections() {
    if (this.isDocumentRTL) {
      return NAVIGATION_DIRECTIONS.RTL;
    }
    return NAVIGATION_DIRECTIONS.LTR;
  }

  get isDocumentRTL() {
    if (typeof Services !== "undefined") {
      return Services.locale.isAppLocaleRTL;
    }
    return document.dir === "rtl";
  }

  navigate(direction) {
    let nextIndex = this.focusableIndex + NAVIGATION_VALUE[direction];

    if (nextIndex < 0) {
      nextIndex = this.#radioButtons.length - 1;
    } else if (nextIndex >= this.#radioButtons.length) {
      nextIndex = 0;
    }

    let nextButton = this.#radioButtons[nextIndex];
    nextButton.click();
  }

  willUpdate(changedProperties) {
    if (changedProperties.has("name")) {
      this.handleSetName();
    }
  }

  handleSetName() {
    this.#radioButtons.forEach(button => {
      button.name = this.name;
    });
  }

  // Re-dispatch change event so it's re-targeted to moz-radio-group.
  handleChange(event) {
    event.stopPropagation();
    this.dispatchEvent(new Event(event.type));
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-radio-group.css"
      />
      <fieldset role="radiogroup">
        <legend class="heading-medium">${this.label}</legend>
        <slot
          @slotchange=${this.syncStateToRadioButtons}
          @change=${this.handleChange}
        ></slot>
      </fieldset>
    `;
  }
}
customElements.define("moz-radio-group", MozRadioGroup);

/**
 * Input element that allows a user to select one option from a group of options.
 *
 * @tagname moz-radio
 * @property {boolean} checked - Whether or not the input is selected.
 * @property {string} label - Label for the radio input.
 * @property {string} name
 *  Name of the input control, set by the associated moz-radio-group element.
 * @property {number} inputTabIndex - Tabindex of the input element.
 * @property {number} value - Value of the radio input.
 */
export class MozRadio extends MozLitElement {
  #controller;

  static properties = {
    checked: { type: Boolean, reflect: true },
    iconSrc: { type: String },
    label: { type: String },
    name: { type: String, attribute: false },
    inputTabIndex: { type: Number, state: true },
    value: { type: String },
  };

  static queries = {
    radioButton: "#radio-button",
    labelEl: "label",
    icon: ".icon",
  };

  constructor() {
    super();
    this.checked = false;
  }

  connectedCallback() {
    super.connectedCallback();
    this.dataset.l10nAttrs = "label";

    let hostRadioGroup = this.parentElement || this.getRootNode().host;
    if (!(hostRadioGroup instanceof MozRadioGroup)) {
      console.error("moz-radio can only be used in moz-radio-group element.");
    }

    this.#controller = hostRadioGroup;
  }

  willUpdate(changedProperties) {
    // Handle setting checked directly via JS.
    if (
      changedProperties.has("checked") &&
      this.checked &&
      this.#controller.value &&
      this.value !== this.#controller.value
    ) {
      this.#controller.value = this.value;
    }
    // Handle un-checking directly via JS. If the checked input is un-checked,
    // the value of the associated moz-radio-group needs to be un-set.
    if (
      changedProperties.has("checked") &&
      !this.checked &&
      this.#controller.value &&
      this.value === this.#controller.value
    ) {
      this.#controller.value = "";
    }
  }

  handleClick() {
    this.#controller.value = this.value;
    this.focus();
  }

  // Re-dispatch change event so it propagates out of moz-radio.
  handleChange(e) {
    this.dispatchEvent(new Event(e.type, e));
  }

  // Delegate click to the input element.
  click() {
    this.radioButton.click();
    this.focus();
  }

  // Delegate focus to the input element.
  focus() {
    this.radioButton.focus();
  }

  iconTemplate() {
    if (this.iconSrc) {
      return html`<img src=${this.iconSrc} role="presentation" class="icon" />`;
    }
    return "";
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-radio.css"
      />
      <label is="moz-label" for="radio-button">
        <input
          type="radio"
          id="radio-button"
          value=${this.value}
          name=${this.name}
          .checked=${this.checked}
          aria-checked=${this.checked}
          tabindex=${this.inputTabIndex}
          @click=${this.handleClick}
          @change=${this.handleChange}
        />
        <span class="label-content">
          ${this.iconTemplate()}
          <span class="text"> ${this.label || html`<slot></slot>`} </span>
        </span>
      </label>
    `;
  }
}
customElements.define("moz-radio", MozRadio);
