/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * A range slider component that can be used to select a value.
 *
 * @tagname moz-slider
 * @property {number} min - The minimum value of the slider.
 * @property {number} max - The maximum value of the slider.
 * @property {number} value - The initial value of the slider.
 * @property {number} ticks - The number of tick marks to display under the slider.
 * @property {Array<string>} tickLabels - A list containing the tick label strings.
 * @property {string} label - The label text for the slider.
 * @property {string} sliderIcon  - The url of the slider icon.
 */

export default class MozSlider extends MozLitElement {
  static properties = {
    min: { type: Number },
    max: { type: Number },
    value: { type: Number },
    ticks: { type: Number },
    tickLabels: { type: Array, attribute: "tick-labels" },
    label: { type: String },
    sliderIcon: { type: String, attribute: "slider-icon" },
  };

  static get queries() {
    return {
      tickMarks: "datalist",
      sliderTrack: "#inputSlider",
    };
  }

  constructor() {
    super();
    this.ticks = 0;
    this.tickLabels = [];
    this.sliderIcon = "";
  }

  getStepSize() {
    const stepSize = (this.max - this.min) / (this.ticks - 1);
    return stepSize;
  }

  setupIcon() {
    if (this.sliderIcon) {
      return html`<div class="icon-container">
        <img class="icon" role="presentation" src=${this.sliderIcon} />
      </div> `;
    }
    return "";
  }

  ticksTemplate() {
    if (this.ticks > 0) {
      let tickList = [];
      let value = this.min;
      let stepSize = this.getStepSize();
      let className = "";
      for (let i = 0; i < this.ticks; i++) {
        let optionId = "";
        let label = "";
        if (this.tickLabels.length) {
          if (i == 0) {
            optionId = "inline-start-label";
            label = this.tickLabels[0];
          } else if (i == this.ticks - 1) {
            optionId = "inline-end-label";
            label = this.tickLabels[1];
          }
        } else {
          className = "no-tick-labels";
        }
        tickList.push(
          html`<option
            id=${optionId}
            value=${parseFloat(value).toFixed(2)}
            label=${label}
          ></option>`
        );
        value += stepSize;
      }
      return html`
        <datalist id="slider-ticks" class=${className}>${tickList}</datalist>
      `;
    }
    return "";
  }

  handleChange(event) {
    this.value = event.target.value;
    this.dispatchEvent(
      new CustomEvent("slider-changed", {
        detail: this.value,
      })
    );
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/reader/moz-slider.css"
      />
      <div class="container">
        <label class="slider-label" for="inputSlider">${this.label}</label>
        ${this.setupIcon()}
        <div class="slider-container">
          <input
            id="inputSlider"
            max=${this.max}
            min=${this.min}
            step=${this.getStepSize()}
            type="range"
            .value=${this.value}
            list="slider-ticks"
            @change=${this.handleChange}
          />
          ${this.ticksTemplate()}
        </div>
      </div>
    `;
  }
}
customElements.define("moz-slider", MozSlider);
