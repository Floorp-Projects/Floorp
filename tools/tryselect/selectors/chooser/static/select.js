/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const labels = $("label.multiselect");
const boxes = $("label.multiselect input:checkbox");
var lastChecked = {};

// implements shift+click
labels.click(function (e) {
  if (e.target.tagName === "INPUT") {
    return;
  }

  let box = $("#" + this.htmlFor)[0];
  let activeSection = $("div.tab-pane.active")[0].id;

  if (activeSection in lastChecked) {
    // Bug 559506 - In Firefox shift/ctrl/alt+clicking a label doesn't check the box.
    let isFirefox = navigator.userAgent.toLowerCase().indexOf("firefox") > -1;

    if (e.shiftKey) {
      if (isFirefox) {
        box.checked = !box.checked;
      }

      let start = boxes.index(box);
      let end = boxes.index(lastChecked[activeSection]);

      boxes
        .slice(Math.min(start, end), Math.max(start, end) + 1)
        .prop("checked", box.checked);
      apply();
    }
  }

  lastChecked[activeSection] = box;
});

function selectAll(btn) {
  let checked = !!btn.value;
  $("div.active label.filter-label").each(function () {
    $(this).find("input:checkbox")[0].checked = checked;
  });
  apply();
}
