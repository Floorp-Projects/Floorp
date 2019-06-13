/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// The HTML tests are in browser_html_warning_messages.js.
SpecialPowers.pushPrefEnv({
  set: [["extensions.htmlaboutaddons.enabled", false]],
});

// Tests that signed and unsigned language packs show up correctly in
// the Languages tab based on the langpack signing preference.
add_task(async function() {
  const PREF = "extensions.langpacks.signatures.required";

  await SpecialPowers.pushPrefEnv({
    set: [[PREF, false]],
  });

  let provider = new MockProvider();

  provider.createAddons([{
    id: "signed@tests.mozilla.org",
    name: "Signed langpack",
    type: "locale",
    signedState: AddonManager.SIGNEDSTATE_SIGNED,
    isCorrectlySigned: true,
  }, {
    id: "unsigned@tests.mozilla.org",
    name: "Unsigned langpack",
    type: "locale",
    signedState: AddonManager.SIGNEDSTATE_MISSING,
    isCorrectlySigned: false,
  }]);


  let mgrWin = await open_manager(null);

  function checklist(signingRequired) {
    let list = mgrWin.document.getElementById("addon-list");
    is(list.itemChildren.length, 2, "Found 2 items in langpack list");
    for (let item of list.itemChildren) {
      let what, warningVisible, errorVisible;

      if (item.mAddon.id.startsWith("signed")) {
        // Signed langpack should not have any warning/error
        what = "signed langpack";
        warningVisible = false;
        errorVisible = false;
      } else if (signingRequired) {
        // Unsigned should have an error if signing is required
        what = "unsigned langpack";
        warningVisible = false;
        errorVisible = true;
      } else {
        // Usnigned should have a warning is signing is not required
        what = "unsigned langpack";
        warningVisible = true;
        errorVisible = false;
      }

      let warning = mgrWin.document.getAnonymousElementByAttribute(item, "anonid", "warning");
      let warningLink = mgrWin.document.getAnonymousElementByAttribute(item, "anonid", "warning-link");
      if (warningVisible) {
        is_element_visible(warning, `Warning should be visible for ${what}`);
        is_element_visible(warningLink, `Warning link should be visible for ${what}`);
      } else {
        is_element_hidden(warning, `Warning should be hidden for ${what}`);
        is_element_hidden(warningLink, `Warning link should be hidden for ${what}`);
      }

      let error = mgrWin.document.getAnonymousElementByAttribute(item, "anonid", "error");
      let errorLink = mgrWin.document.getAnonymousElementByAttribute(item, "anonid", "error-link");
      if (errorVisible) {
        is_element_visible(error, `Error should be visible for ${what}`);
        is_element_visible(errorLink, `Error link should be visible for ${what}`);
      } else {
        is_element_hidden(error, `Error should be hidden for ${what}`);
        is_element_hidden(errorLink, `Error link should be hidden for ${what}`);
      }
    }
  }

  let catUtils = new CategoryUtilities(mgrWin);

  await catUtils.openType("locale");
  checklist(false);

  await SpecialPowers.pushPrefEnv({
    set: [[PREF, true]],
  });

  await catUtils.openType("extension");
  await catUtils.openType("locale");
  checklist(true);

  await close_manager(mgrWin);
});
