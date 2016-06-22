/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Color"];

/**
 * Color class, which describes a color.
 * In the future, this object may be extended to allow for conversions between
 * different color formats and notations, support transparency.
 *
 * @param {Number} r Red color component
 * @param {Number} g Green color component
 * @param {Number} b Blue color component
 */
function Color(r, g, b) {
  this.r = r;
  this.g = g;
  this.b = b;
}

Color.prototype = {
  /**
   * Formula from W3C's WCAG 2.0 spec's relative luminance, section 1.4.1,
   * http://www.w3.org/TR/WCAG20/.
   *
   * @return {Number} Relative luminance, represented as number between 0 and 1.
   */
  get relativeLuminance() {
    let colorArr = [this.r, this.b, this.g].map(color => {
      color = parseInt(color, 10);
      if (color <= 10)
        return color / 255 / 12.92;
      return Math.pow(((color / 255) + 0.055) / 1.055, 2.4);
    });
    return colorArr[0] * 0.2126 +
           colorArr[1] * 0.7152 +
           colorArr[2] * 0.0722;
  },

  /**
   * @return {Boolean} TRUE if the color value can be considered bright.
   */
  get isBright() {
    // Note: this is a high enough value to be considered as 'bright', but was
    //       decided upon empirically.
    return this.relativeLuminance > 0.7;
  },

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
    if (!(otherColor instanceof Color))
      throw new TypeError("The first argument should be an instance of Color");

    let luminance = this.relativeLuminance;
    let otherLuminance = otherColor.relativeLuminance;
    return (Math.max(luminance, otherLuminance) + 0.05) /
      (Math.min(luminance, otherLuminance) + 0.05);
  },

  /**
   * Biased method to check if the contrast ratio between two colors is high
   * enough to be discernable.
   *
   * @param  {Color} otherColor Color instance to calculate the contrast with
   * @return {Boolean}
   */
  isContrastRatioAcceptable(otherColor) {
    // Note: this is a high enough value to be considered as 'high contrast',
    //       but was decided upon empirically.
    return this.contrastRatio(otherColor) > 3;
  }
};
