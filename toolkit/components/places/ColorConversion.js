/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Given a color in the Lab space, return its chroma (colorfulness,
 * saturation).
 *
 * @param lab
 *        The lab color to get the chroma from
 *
 * @return A number greater than zero that measures chroma in the image
 */
function labChroma(lab) {
  return Math.sqrt(Math.pow(lab.a, 2) + Math.pow(lab.b, 2));
}

/**
 * Given the RGB components of a color as integers from 0-255, return the
 * color in the XYZ color space.
 *
 * @return An object with x, y, z properties holding those components of the
 *         color in the XYZ color space.
 */
function rgb2xyz(r, g, b) {
  r /= 255;
  g /= 255;
  b /= 255;

  // assume sRGB
  r = r > 0.04045 ? Math.pow(((r + 0.055) / 1.055), 2.4) : (r / 12.92);
  g = g > 0.04045 ? Math.pow(((g + 0.055) / 1.055), 2.4) : (g / 12.92);
  b = b > 0.04045 ? Math.pow(((b + 0.055) / 1.055), 2.4) : (b / 12.92);

  return {
    x: ((r * 0.4124) + (g * 0.3576) + (b * 0.1805)) * 100,
    y: ((r * 0.2126) + (g * 0.7152) + (b * 0.0722)) * 100,
    z: ((r * 0.0193) + (g * 0.1192) + (b * 0.9505)) * 100
  };
}

/**
 * Given the RGB components of a color as integers from 0-255, return the
 * color in the Lab color space.
 *
 * @return An object with lightness, a, b properties holding those components
 *         of the color in the Lab color space.
 */
function rgb2lab(r, g, b) {
  let xyz = rgb2xyz(r, g, b),
        x = xyz.x / 95.047,
        y = xyz.y / 100,
        z = xyz.z / 108.883;

  x = x > 0.008856 ? Math.pow(x, 1/3) : (7.787 * x) + (16 / 116);
  y = y > 0.008856 ? Math.pow(y, 1/3) : (7.787 * y) + (16 / 116);
  z = z > 0.008856 ? Math.pow(z, 1/3) : (7.787 * z) + (16 / 116);

  return {
    lightness: (116 * y) - 16,
    a:  500 * (x - y),
    b:  200 * (y - z)
  };
}
