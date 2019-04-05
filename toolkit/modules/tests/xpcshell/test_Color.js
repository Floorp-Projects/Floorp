"use strict";

const {Color} = ChromeUtils.import("resource://gre/modules/Color.jsm");

function run_test() {
  testRelativeLuminance();
  testUseBrightText();
  testContrastRatio();
  testIsContrastRatioAcceptable();
}

function testRelativeLuminance() {
  let c = new Color(0, 0, 0);
  Assert.equal(c.relativeLuminance, 0, "Black is not illuminating");

  c = new Color(255, 255, 255);
  Assert.equal(c.relativeLuminance, 1, "White is quite the luminant one");

  c = new Color(142, 42, 142);
  Assert.equal(c.relativeLuminance, 0.09359705837110571, "This purple is not that luminant");
}

function testUseBrightText() {
  let c = new Color(0, 0, 0);
  Assert.ok(c.useBrightText, "Black is bright, so bright text should be used here");

  c = new Color(255, 255, 255);
  Assert.ok(!c.useBrightText, "White is bright, so better not use bright colored text on it");
}

function testContrastRatio() {
  let c = new Color(0, 0, 0);
  let c2 = new Color(255, 255, 255);
  Assert.equal(c.contrastRatio(c2), 21, "Contrast between black and white is max");
  Assert.equal(c.contrastRatio(c), 1, "Contrast between equals is min");

  let c3 = new Color(142, 42, 142);
  Assert.equal(c.contrastRatio(c3), 2.871941167422114, "Contrast between black and purple shouldn't be very high");
  Assert.equal(c2.contrastRatio(c3), 7.312127503938331, "Contrast between white and purple should be high");
}

function testIsContrastRatioAcceptable() {
  // Let's assert what browser.js is doing for window frames.
  let c = new Color(...[55, 156, 152]);
  let c2 = new Color(0, 0, 0);
  Assert.equal(c.r, 55, "Reds should match");
  Assert.equal(c.g, 156, "Greens should match");
  Assert.equal(c.b, 152, "Blues should match");
  Assert.ok(c.isContrastRatioAcceptable(c2), "The blue is high contrast enough");
  c2 = new Color(...[35, 65, 100]);
  Assert.ok(!c.isContrastRatioAcceptable(c2), "The blue is not high contrast enough");
  // But would it be high contrast enough at a lower conformance level?
  Assert.ok(c.isContrastRatioAcceptable(c2, "A"), "The blue is high contrast enough when used as large text");
}
