/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function () {
  let input = document.createElement("input");
  input.type = "date";
  registerCleanupFunction(() => input.remove());
  document.body.appendChild(input);

  let shown = BrowserTestUtils.waitForDateTimePickerPanelShown(window);

  const shadowRoot = SpecialPowers.wrap(input).openOrClosedShadowRoot;

  EventUtils.synthesizeMouseAtCenter(
    shadowRoot.getElementById("calendar-button"),
    {}
  );

  let popup = await shown;
  ok(!!popup, "Should've shown the popup");

  let hidden = BrowserTestUtils.waitForPopupEvent(popup, "hidden");
  popup.hidePopup();

  await hidden;
  popup.remove();
});
