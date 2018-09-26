/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// "about:ab" should match "about:about"
add_task(async function aboutAb() {
  await check_autocomplete({
    search: "about:ab",
    autofilled: "about:about",
    completed: "about:about",
    matches: [{
      value: "about:about",
      comment: "about:about",
      style: ["autofill", "heuristic"],
    }],
  });
});

// "about:Ab" should match "about:about"
add_task(async function aboutAb() {
  await check_autocomplete({
    search: "about:Ab",
    autofilled: "about:About",
    completed: "about:about",
    matches: [{
      value: "about:about",
      comment: "about:about",
      style: ["autofill", "heuristic"],
    }],
  });
});

// "about:about" should match "about:about"
add_task(async function aboutAbout() {
  await check_autocomplete({
    search: "about:about",
    autofilled: "about:about",
    completed: "about:about",
    matches: [{
      value: "about:about",
      comment: "about:about",
      style: ["autofill", "heuristic"],
    }],
  });
});

// "about:a" should complete to "about:about" and also match "about:addons"
add_task(async function aboutAboutAndAboutAddons() {
  await check_autocomplete({
    search: "about:a",
    autofilled: "about:about",
    completed: "about:about",
    matches: [{
      value: "about:about",
      comment: "about:about",
      style: ["autofill", "heuristic"],
    }, {
      value: "about:addons",
      comment: "about:addons",
    }],
  });
});

// "about:" should *not* match anything
add_task(async function aboutColonHasNoMatch() {
  await check_autocomplete({
    search: "about:",
    autofilled: "about:",
    completed: "about:",
    matches: [],
  });
});
