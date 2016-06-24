"use strict";

Components.utils.import("resource://gre/modules/Color.jsm");

function run_test() {
  testRelativeLuminance();
  testIsBright();
  testContrastRatio();
  testIsContrastRatioAcceptable();
}

function testRelativeLuminance() {
  let c = new Color(0, 0, 0);
  Assert.equal(c.relativeLuminance, 0, "Black is not illuminating");

  c = new Color(255, 255, 255);
  Assert.equal(c.relativeLuminance, 1, "White is quite the luminant one");

  c = new Color(142, 42, 142);
  Assert.equal(c.relativeLuminance, 0.25263952353998204,
    "This purple is not that luminant");
}

function testIsBright() {
  let c = new Color(0, 0, 0);
  Assert.equal(c.isBright, 0, "Black is bright");

  c = new Color(255, 255, 255);
  Assert.equal(c.isBright, 1, "White is bright");
}

function testContrastRatio() {
  let c = new Color(0, 0, 0);
  let c2 = new Color(255, 255, 255);
  Assert.equal(c.contrastRatio(c2), 21, "Contrast between black and white is max");
  Assert.equal(c.contrastRatio(c), 1, "Contrast between equals is min");

  let c3 = new Color(142, 42, 142);
  Assert.equal(c.contrastRatio(c3), 6.05279047079964, "Contrast between black and purple");
  Assert.equal(c2.contrastRatio(c3), 3.469474137806338, "Contrast between white and purple");
}

function testIsContrastRatioAcceptable() {
  // Let's assert what browser.js is doing for window frames.
  let c = new Color(...[55, 156, 152]);
  let c2 = new Color(0, 0, 0);
  Assert.equal(c.r, 55, "Reds should match");
  Assert.equal(c.g, 156, "Greens should match");
  Assert.equal(c.b, 152, "Blues should match");
  Assert.ok(c.isContrastRatioAcceptable(c2), "The blue is high contrast enough");
  c = new Color(...[35, 65, 100]);
  Assert.ok(!c.isContrastRatioAcceptable(c2), "The blue is not high contrast enough");
}
