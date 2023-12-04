/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the translations button becomes disabled when entering pdf.
 */
add_task(async function test_translations_button_disabled_in_pdf() {
  const { cleanup } = await loadTestPage({
    page: EMPTY_PDF_URL,
  });

  const appMenuButton = document.getElementById("PanelUI-menu-button");

  click(appMenuButton, "Opening the app menu");
  await BrowserTestUtils.waitForEvent(window.PanelUI.mainView, "ViewShown");

  const translateSiteButton = document.getElementById(
    "appMenu-translate-button"
  );
  is(
    translateSiteButton.disabled,
    true,
    "The app-menu translate button should be disabled because PDFs are restricted"
  );

  click(appMenuButton, "Closing the app menu");

  await cleanup();
});
