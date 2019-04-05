/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Color"];

/**
 * A list of minimum contrast ratio's per WCAG 2.0 conformance level.
 * Please refer to section 1.4.3 of the WCAG 2.0 spec at http://www.w3.org/TR/WCAG20/.
 * Simply put:
 * A = Large text only.
 * AA = Regular sized text or large text in enhanced contrast mode.
 * AAA = Regular sized text in enhanced contrast mode.
 *
 * @type {Object}
 */
const CONTRAST_RATIO_LEVELS = {
  A: 3,
  AA: 4.5,
  AAA: 7,
};

/**
 * For text legibility on any background color, you need to determine which text
 * color - black or white - will yield the highest contrast ratio.
 * Since you're always comparing `contrastRatio(bgcolor, black) >
 * contrastRatio(bgcolor, white) ? <use black> : <use white>`, we can greatly
 * simplify the calculation to the following constant.
 *
 * @type {Number}
 */
const CONTRAST_BRIGHTTEXT_THRESHOLD = Math.sqrt(1.05 * 0.05) - 0.05;

/**
 * Color class, which describes a color.
 * In the future, this object may be extended to allow for conversions between
 * different color formats and notations, support transparency.
 *
 * @param {Number} r Red color component
 * @param {Number} g Green color component
 * @param {Number} b Blue color component
 */
class Color {
  constructor(r, g, b) {
    this.r = r;
    this.g = g;
    this.b = b;
  }

  /**
   * Formula from W3C's WCAG 2.0 spec's relative luminance, section 1.4.1,
   * http://www.w3.org/TR/WCAG20/.
   *
   * @return {Number} Relative luminance, represented as number between 0 and 1.
   */
  get relativeLuminance() {
    let colorArr = [this.r, this.g, this.b].map(color => {
      color = parseInt(color, 10);
      if (color <= 10) {
        return color / 255 / 12.92;
      }
      return Math.pow(((color / 255) + 0.055) / 1.055, 2.4);
    });
    return colorArr[0] * 0.2126 +
           colorArr[1] * 0.7152 +
           colorArr[2] * 0.0722;
  }

  /**
   * @return {Boolean} TRUE if you need to use a bright color (e.g. 'white'), when
   *                   this color is set as the background.
   */
  get useBrightText() {
    return this.relativeLuminance <= CONTRAST_BRIGHTTEXT_THRESHOLD;
  }

  /**
   * Get the contrast ratio between the current color and a second other color.
   * A common use case is to express the difference between a foreground and a
   * background color in numbers.
   * Formula from W3C's WCAG 2.0 spec's contrast ratio, section 1.4.1,
   * http://www.w3.org/TR/WCAG20/.
   *
   * @param  {Color}  otherColor Color instance to calculate the contrast with
   * @return {Number} Contrast ratios can range from 1 to 21, commonly written
   *                  as 1:1 to 21:1.
   */
  contrastRatio(otherColor) {
    if (!(otherColor instanceof Color)) {
      throw new TypeError("The first argument should be an instance of Color");
    }

    let luminance = this.relativeLuminance;
    let otherLuminance = otherColor.relativeLuminance;
    return (Math.max(luminance, otherLuminance) + 0.05) /
      (Math.min(luminance, otherLuminance) + 0.05);
  }

  /**
   * Method to check if the contrast ratio between two colors is high enough to
   * be discernable.
   *
   * @param  {Color}  otherColor Color instance to calculate the contrast with
   * @param  {String} [level]    WCAG conformance level that maps to the minimum
   *                             required contrast ratio. Defaults to 'AA'
   * @return {Boolean}
   */
  isContrastRatioAcceptable(otherColor, level = "AA") {
    return this.contrastRatio(otherColor) > CONTRAST_RATIO_LEVELS[level];
  }
}
