/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * nodejs script to generate: testing/talos/talos/tests/devtools/addon/content/pages/custom/inspector/index.html
 *
 * Execute it like this:
 * $ nodejs generate-inspector-index-html.js > testing/talos/talos/tests/devtools/addon/content/pages/custom/inspector/index.html
 */

// We first create a deep tree with ${deep} nested children
let deep = 50;
// Then we create ${n} element after the deep tree
let n = 50;
// Number of attributes set on the repeated elements
let attributes = 50;

// Build the <div> with $attributes data attributes
let div = "<div";
for (var i = 1; i <= attributes; i++) {
  div += ` data-a${i}="${i}"`;
}
div += ">";

// Build the tree of $deep elements
let tree = "";
for (i = 1; i <= deep; i++) {
  tree += new Array(i).join(" ");
  tree += div + " " + i + "\n";
}
for (i = deep; i >= 1; i--) {
  tree += new Array(i).join(" ");
  tree += "</div>\n";
}

// Build the list of $n elements
let repeat = "";
for (i = 1; i <= n; i++) {
  repeat += div + " " + i + " </div>\n";
}

// Prepare CSS rules to add to the document <style>.
let CSS_RULES_COUNT = 200;
let manyCssRules = "";
for (i = 0; i < CSS_RULES_COUNT; i++) {
  manyCssRules += `
  .many-css-rules {
    font-size: ${i}px;
    margin: 10px;
    padding: 10px;
    font-family: Arial;
    margin: 20px;
  }`;
}
let expandManyChildren = new Array(100).join("  <div attr='my-attr'>content</div>\n");

let maxBalancedDepth = 8;
function createBalancedMarkup(level = 0) {
  let tab = new Array(level + 1).join("  ");
  if (level < maxBalancedDepth) {
    let child = createBalancedMarkup(level + 1);
    return `${tab}<div>
${child}
${child}
${tab}</div>`;
  } else {
    return tab + "<div class='leaf'>leaf</div>";
  }
}
let expandBalanced = createBalancedMarkup();

console.log(`
<!DOCTYPE html>
<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/.  -->
<!-- This file is a generated file, do not edit it directly.
   - See generate-inspector-html.js for instructions to update this file -->
<html>
<head>
  <meta charset="utf-8">
  <title>Custom page for the Inspector</title>
  <style>
  div {
    margin-left: 0.5em;
  }
  /* Styles for custom.inspector.manyrules tests */`);
console.log(manyCssRules);
console.log(`
  </style>
</head>
<body>
<!-- <div> elements with ${deep} nested childs, all with ${attributes} attributes -->
<!-- The deepest <div> has id="deep"> -->
`);
console.log(tree);
console.log(`
<!-- ${n} <div> elements without any children -->
`);
console.log(repeat);
console.log(`
<!-- Elements for custom.inspector.manyrules tests -->
<div class="no-css-rules"></div>
<div class="many-css-rules"></div>
<div class="expand-many-children">
`);
console.log(expandManyChildren);
console.log(`
</div>
<div class="expand-balanced">
`);
console.log(expandBalanced);
console.log(`
</div>
</body>
</html>`);

