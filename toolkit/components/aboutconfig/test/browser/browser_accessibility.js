/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const MAX_PLACEABLE_LENGTH = 2500;

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["test.aboutconfig.added", "=".repeat(MAX_PLACEABLE_LENGTH)],
      ["test.aboutconfig.long", "=".repeat(MAX_PLACEABLE_LENGTH + 1)],
    ],
  });
});

add_task(async function test_accessible_value() {
  await AboutConfigTest.withNewTab(async function() {
    for (let [name, expectHasUserValue] of [
      [PREF_BOOLEAN_DEFAULT_TRUE, false],
      [PREF_BOOLEAN_USERVALUE_TRUE, true],
      ["test.aboutconfig.added", true],
    ]) {
      let span = this.getRow(name).valueCell.querySelector("span");
      let expectedL10nId = expectHasUserValue
        ? "about-config-pref-accessible-value-custom"
        : "about-config-pref-accessible-value-default";
      Assert.equal(span.getAttribute("data-l10n-id"), expectedL10nId);
    }

    // If the value is too long for localization, the state is not included.
    let span = this.getRow("test.aboutconfig.long").valueCell.querySelector(
      "span"
    );
    Assert.ok(!span.hasAttribute("data-l10n-id"));
    Assert.equal(
      span.getAttribute("aria-label"),
      Preferences.get("test.aboutconfig.long")
    );
  });
});
