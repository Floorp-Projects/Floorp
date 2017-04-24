
add_task(async function() {
  let extensions = [
    {
      id: "mpc@tests.mozilla.org",
      name: "Compatible extension",
      multiprocessCompatible: true,
    },
    {
      id: "not-mpc@tests.mozilla.org",
      name: "Incompatible extension",
      multiprocessCompatible: false,
    },
  ];

  let provider = new MockProvider();
  provider.createAddons(extensions);

  let mgrWin = await open_manager(null);
  let catUtils = new CategoryUtilities(mgrWin);

  async function check(name, disabled) {
    await catUtils.openType("extension");

    let document = mgrWin.document;
    // First find the extension entry in the extensions list.
    let item = Array.from(document.getElementById("addon-list").childNodes)
                    .find(i => i.getAttribute("name") == name);

    ok(item, `Found ${name} in extensions list`);
    item.parentNode.ensureElementIsVisible(item);

    // Check individual elements on this item.
    let disabledPostfix = document.getAnonymousElementByAttribute(item, "class", "disabled-postfix");
    let enableBtn = document.getAnonymousElementByAttribute(item, "anonid", "enable-btn");
    let disableBtn = document.getAnonymousElementByAttribute(item, "anonid", "disable-btn");
    let errorMsg = document.getAnonymousElementByAttribute(item, "anonid", "error");
    let errorLink = document.getAnonymousElementByAttribute(item, "anonid", "error-link");

    if (disabled) {
      is_element_visible(disabledPostfix, "Disabled postfix should be visible");
      is_element_hidden(enableBtn, "Enable button should be hidden");
      is_element_hidden(disableBtn, "Disable button should be hidden");
      is_element_visible(errorMsg, "Error message should be visible");
      is_element_visible(errorLink, "Error link should be visible");
    } else {
      is_element_hidden(disabledPostfix, "Disabled postfix should be hidden");
      is_element_hidden(enableBtn, "Enable button should be hidden");
      is_element_visible(disableBtn, "Disable button should be visible");
      is_element_hidden(errorMsg, "Error message should be hidden");
      is_element_hidden(errorLink, "Error link should be hidden");
    }

    // Click down to the details page.
    EventUtils.synthesizeMouseAtCenter(item, {clickCount: 2}, mgrWin);
    await new Promise(resolve => wait_for_view_load(mgrWin, resolve));

    // And check its contents.
    enableBtn = mgrWin.document.getElementById("detail-enable-btn");
    disableBtn = mgrWin.document.getElementById("detail-disable-btn");
    errorMsg = mgrWin.document.getElementById("detail-error");

    if (disabled) {
      is_element_hidden(enableBtn, "Enable button should be hidden");
      is_element_hidden(disableBtn, "Disable button should be hidden");
      is_element_visible(errorMsg, "Error message should be visible");
    } else {
      is_element_hidden(enableBtn, "Enable button should be hidden");
      is_element_visible(disableBtn, "Disable button should be visible");
      is_element_hidden(errorMsg, "Error message should be hidden");
    }
  }

  // Initially, both extensions should be enabled
  await check("Compatible extension", false);
  await check("Incompatible extension", false);

  // Flip the pref, making the non-MPC extension disabled.
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.allow-non-mpc-extensions", false]],
  });
  extensions[1].multiprocessCompatible = false;
  extensions[1].appDisabled = true;

  // The compatible extensions should be unaffected, the incompatible
  // one should have the error message etc.
  check("Compatible extension", false);
  check("Incompatible extension", true);

  await close_manager(mgrWin);
});
