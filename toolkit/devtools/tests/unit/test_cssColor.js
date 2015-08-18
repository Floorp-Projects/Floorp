/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test classifyColor.

"use strict";

const loader = new DevToolsLoader();
const require = loader.require;
const {colorUtils} = require("devtools/css-color");

const CLASSIFY_TESTS = [
  { input: "rgb(255,0,192)", output: "rgb" },
  { input: "RGB(255,0,192)", output: "rgb" },
  { input: "rgba(255,0,192, 0.25)", output: "rgb" },
  { input: "hsl(5, 5, 5)", output: "hsl" },
  { input: "hsla(5, 5, 5, 0.25)", output: "hsl" },
  { input: "hSlA(5, 5, 5, 0.25)", output: "hsl" },
  { input: "#f0c", output: "hex" },
  { input: "#fe01cb", output: "hex" },
  { input: "#FE01CB", output: "hex" },
  { input: "blue", output: "name" },
  { input: "orange", output: "name" }
];

function run_test() {
  for (let test of CLASSIFY_TESTS) {
    let result = colorUtils.classifyColor(test.input);
    equal(result, test.output, "test classifyColor(" + test.input + ")");
  }
}
