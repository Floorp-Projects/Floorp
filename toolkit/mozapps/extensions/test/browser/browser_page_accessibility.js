/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testPageTitle() {
  let win = await loadInitialView("extension");
  let title = win.document.querySelector("title");
  is(
    win.document.l10n.getAttributes(title).id,
    "addons-page-title",
    "The page title is set"
  );
  await closeView(win);
});
