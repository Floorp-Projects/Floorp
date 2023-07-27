/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ifDefined, html } from "../vendor/lit.all.mjs";
import { MozLitElement } from "../lit-utils.mjs";

window.MozXULElement?.insertFTLIfNeeded("toolkit/global/mozFiveStar.ftl");

/**
 * The visual representation is five stars, each of them either empty,
 * half-filled or full. The fill state is derived from the rating,
 * rounded to the nearest half.
 *
 * @tagname moz-five-star
 * @property {number} rating - The rating out of 5.
 * @property {string} title - The title text.
 */
export default class MozFiveStar extends MozLitElement {
  static properties = {
    rating: { type: Number, reflect: true },
    title: { type: String },
  };

  static get queries() {
    return {
      starEls: { all: ".rating-star" },
      starsWrapperEl: ".stars",
    };
  }

  // Use a relative URL in storybook to get faster reloads on style changes.
  static stylesheetUrl = window.IS_STORYBOOK
    ? "./moz-five-star/moz-five-star.css"
    : "chrome://global/content/elements/moz-five-star.css";

  getStarsFill() {
    let starFill = [];
    let roundedRating = Math.round(this.rating * 2) / 2;
    for (let i = 1; i <= 5; i++) {
      if (i <= roundedRating) {
        starFill.push("full");
      } else if (i - roundedRating === 0.5) {
        starFill.push("half");
      } else {
        starFill.push("empty");
      }
    }
    return starFill;
  }

  render() {
    let starFill = this.getStarsFill();
    return html`
      <link rel="stylesheet" href=${this.constructor.stylesheetUrl} />
      <div
        class="stars"
        data-l10n-id=${ifDefined(this.title ? null : "moz-five-star-rating")}
        data-l10n-args=${ifDefined(
          this.title ? null : JSON.stringify({ rating: this.rating ?? 0 })
        )}
      >
        ${starFill.map(
          fill => html`<span class="rating-star" fill="${fill}"></span>`
        )}
      </div>
    `;
  }
}
customElements.define("moz-five-star", MozFiveStar);
