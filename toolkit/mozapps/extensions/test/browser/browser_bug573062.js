/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  waitForExplicitFinish();

  var gProvider = new MockProvider();
  let perms = AddonManager.PERM_CAN_UNINSTALL |
              AddonManager.PERM_CAN_ENABLE | AddonManager.PERM_CAN_DISABLE;

  gProvider.createAddons([{
    id: "restart-enable-disable@tests.mozilla.org",
    name: "restart-enable-disable",
    description: "foo",
    permissions: perms,
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_ENABLE |
                                AddonManager.OP_NEEDS_RESTART_DISABLE
  },
  {
    id: "restart-uninstall@tests.mozilla.org",
    name: "restart-uninstall",
    description: "foo",
    permissions: perms,
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_UNINSTALL
  },
  {
    id: "no-restart-required@tests.mozilla.org",
    name: "no-restart-required",
    description: "bar",
    permissions: perms,
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE
  }]);
  
  open_manager("addons://list/extension", function(aWindow) {
    let addonList = aWindow.document.getElementById("addon-list");
    let ed_r_Item, un_r_Item, no_r_Item;
    for (let i = 0; i < addonList.childNodes.length; i++) {
      let addonItem = addonList.childNodes[i];
      let name = addonItem.getAttribute("name");
      switch (name) {
        case "restart-enable-disable":
          ed_r_Item = addonItem;
          break;
        case "restart-uninstall":
          un_r_Item = addonItem;
          break;
        case "no-restart-required":
          no_r_Item = addonItem;
          break;
      }
    }

    // Check the buttons in the list view.
    function checkTooltips(aItem, aEnable, aDisable, aRemove) {
      ok(aItem._enableBtn.getAttribute("tooltiptext") == aEnable);
      ok(aItem._disableBtn.getAttribute("tooltiptext") == aDisable);
      ok(aItem._removeBtn.getAttribute("tooltiptext")  == aRemove);
    }

    let strs = aWindow.gStrings.ext;
    addonList.selectedItem = ed_r_Item;
    let ed_args = [ed_r_Item,
                   strs.GetStringFromName("enableAddonRestartRequiredTooltip"),
                   strs.GetStringFromName("disableAddonRestartRequiredTooltip"),
                   strs.GetStringFromName("uninstallAddonTooltip")];
    checkTooltips.apply(null, ed_args);

    addonList.selectedItem = un_r_Item;
    let un_args = [un_r_Item,
                   strs.GetStringFromName("enableAddonTooltip"),
                   strs.GetStringFromName("disableAddonTooltip"),
                   strs.GetStringFromName("uninstallAddonRestartRequiredTooltip")];
    checkTooltips.apply(null, un_args);

    addonList.selectedItem = no_r_Item;
    let no_args = [no_r_Item,
                   strs.GetStringFromName("enableAddonTooltip"),
                   strs.GetStringFromName("disableAddonTooltip"),
                   strs.GetStringFromName("uninstallAddonTooltip")];
    checkTooltips.apply(null, no_args)

    // Check the buttons in the details view.
    function checkTooltips2(aItem, aEnable, aDisable, aRemove) {
        let detailEnable = aWindow.document.getElementById("detail-enable");
    let detailDisable = aWindow.document.getElementById("detail-disable");
    let detailUninstall = aWindow.document.getElementById("detail-uninstall");
      ok(detailEnable.getAttribute("tooltiptext") == aEnable);
      ok(detailDisable.getAttribute("tooltiptext") == aDisable);
      ok(detailUninstall.getAttribute("tooltiptext")  == aRemove);
    }

    function showInDetailView(aAddonId) {
      aWindow.gViewController.loadView("addons://detail/" +
                                       aWindow.encodeURIComponent(aAddonId));
    }

    // enable-disable:
    showInDetailView("restart-enable-disable@tests.mozilla.org");
    wait_for_view_load(aWindow, function() {
      checkTooltips2.apply(null, ed_args);
      // uninstall:
      showInDetailView("restart-uninstall@tests.mozilla.org");
      wait_for_view_load(aWindow, function() {
        checkTooltips2.apply(null, un_args);
        // no restart:
        showInDetailView("no-restart-required@tests.mozilla.org");
        wait_for_view_load(aWindow, function() {
          checkTooltips2.apply(null, no_args);
          aWindow.close();
          finish();
        });
      });
    });

  });
}
