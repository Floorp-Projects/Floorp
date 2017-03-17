/* exported BACKGROUND, ACCENT_COLOR, TEXT_COLOR, hexToRGB */

"use strict";

const BACKGROUND = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";
const ACCENT_COLOR = "#a14040";
const TEXT_COLOR = "#fac96e";

function hexToRGB(hex) {
  hex = parseInt((hex.indexOf("#") > -1 ? hex.substring(1) : hex), 16);
  return [hex >> 16, (hex & 0x00FF00) >> 8, (hex & 0x0000FF)];
}
