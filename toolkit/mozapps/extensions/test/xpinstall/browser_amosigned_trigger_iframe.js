// ----------------------------------------------------------------------------
// Test for bug 589598 - Ensure that installing through InstallTrigger
// works in an iframe in web content.

function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.finalContentEvent = "InstallComplete";
  Harness.setup();

  PermissionTestUtils.add(
    "http://example.com/",
    "install",
    Services.perms.ALLOW_ACTION
  );

  var inner_url = encodeURIComponent(
    TESTROOT +
      "installtrigger.html?" +
      encodeURIComponent(
        JSON.stringify({
          "Unsigned XPI": {
            URL: TESTROOT + "amosigned.xpi",
            IconURL: TESTROOT + "icon.png",
            toString() {
              return this.URL;
            },
          },
        })
      )
  );
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.loadURI(
    gBrowser,
    TESTROOT + "installtrigger_frame.html?" + inner_url
  );
}

function confirm_install(panel) {
  is(panel.getAttribute("name"), "XPI Test", "Should have seen the name");
  return true;
}

function install_ended(install, addon) {
  return addon.uninstall();
}

const finish_test = async function(count) {
  is(count, 1, "1 Add-on should have been successfully installed");

  PermissionTestUtils.remove("http://example.com", "install");

  const results = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      return {
        return: content.frames[0].document.getElementById("return").textContent,
        status: content.frames[0].document.getElementById("status").textContent,
      };
    }
  );

  is(
    results.return,
    "true",
    "installTrigger in iframe should have claimed success"
  );
  is(results.status, "0", "Callback in iframe should have seen a success");

  gBrowser.removeCurrentTab();
  Harness.finish();
};
