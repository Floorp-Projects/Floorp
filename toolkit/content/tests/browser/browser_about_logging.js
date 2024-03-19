const PAGE = "about:logging";

function clearLoggingPrefs() {
  for (let pref of Services.prefs.getBranch("logging.").getChildList("")) {
    info(`Clearing: ${pref}`);
    Services.prefs.clearUserPref("logging." + pref);
  }
}

// Before running, save any MOZ_LOG environment variable that might be preset,
// and restore them at the end of this test.
add_setup(async function saveRestoreLogModules() {
  let savedLogModules = Services.env.get("MOZ_LOG");
  Services.env.set("MOZ_LOG", "");
  registerCleanupFunction(() => {
    clearLoggingPrefs();
    info(" -- Restoring log modules: " + savedLogModules);
    for (let pref of savedLogModules.split(",")) {
      let [logModule, level] = pref.split(":");
      Services.prefs.setIntPref("logging." + logModule, parseInt(level));
    }
    // Removing this line causes a sandboxxing error in nsTraceRefCnt.cpp (!).
    Services.env.set("MOZ_LOG", savedLogModules);
  });
});

// Test that some UI elements are disabled in some cirumstances.
add_task(async function testElementsDisabled() {
  // This test needs a MOZ_LOG env var set.
  Services.env.set("MOZ_LOG", "example:4");
  await BrowserTestUtils.withNewTab(PAGE, async browser => {
    await SpecialPowers.spawn(browser, [], async () => {
      let $ = content.document.querySelector.bind(content.document);
      Assert.ok(
        $("#set-log-modules-button").disabled,
        "Because a MOZ_LOG env var is set by the harness, it should be impossible to set new log modules."
      );
    });
  });
  Services.env.set("MOZ_LOG", "");

  await BrowserTestUtils.withNewTab(
    PAGE + "?modules=example:5&output=profiler",
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        let $ = content.document.querySelector.bind(content.document);
        Assert.ok(
          !$("#some-elements-unavailable").hidden,
          "If a log modules are configured via URL params, a warning should be visible."
        );
        Assert.ok(
          $("#set-log-modules-button").disabled,
          "If a log modules are configured via URL params, some in-page elements should be disabled (button)."
        );
        Assert.ok(
          $("#log-modules").disabled,
          "If a log modules are configured via URL params, some in-page elements should be disabled (input)."
        );
        Assert.ok(
          $("#logging-preset-dropdown").disabled,
          "If a log modules are configured via URL params, some in-page elements should be disabled (dropdown)."
        );
        Assert.ok(
          $("#radio-logging-profiler").disabled &&
            $("#radio-logging-file").disabled,
          "If the ouptut type is configured via URL param, the radio buttons should be disabled."
        );
      });
    }
  );
  await BrowserTestUtils.withNewTab(
    PAGE + "?preset=media-playback",
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        let $ = content.document.querySelector.bind(content.document);
        Assert.ok(
          !$("#some-elements-unavailable").hidden,
          "If a preset is selected via URL, a warning should be displayed."
        );
        Assert.ok(
          $("#set-log-modules-button").disabled,
          "If a preset is selected via URL, some in-page elements should be disabled (button)."
        );
        Assert.ok(
          $("#log-modules").disabled,
          "If a preset is selected via URL, some in-page elements should be disabled (input)."
        );
        Assert.ok(
          $("#logging-preset-dropdown").disabled,
          "If a preset is selected via URL, some in-page elements should be disabled (dropdown)."
        );
      });
    }
  );
  clearLoggingPrefs();
});

// Test URL parameters
const modulesInURL = "example:4,otherexample:5";
const presetInURL = "media-playback";
const threadsInURL = "example,otherexample";
const profilerPresetInURL = "media";
add_task(async function testURLParameters() {
  await BrowserTestUtils.withNewTab(
    PAGE + "?modules=" + modulesInURL,
    async browser => {
      await SpecialPowers.spawn(browser, [modulesInURL], async modulesInURL => {
        let $ = content.document.querySelector.bind(content.document);
        Assert.ok(
          !$("#some-elements-unavailable").hidden,
          "If modules are selected via URL, a warning should be displayed."
        );
        var inInputSorted = $("#log-modules").value.split(",").sort().join(",");
        var modulesSorted = modulesInURL.split(",").sort().join(",");
        Assert.equal(
          modulesSorted,
          inInputSorted,
          "When selecting modules via URL params, the log modules aren't immediately set"
        );
      });
    }
  );
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE + "?preset=" + presetInURL,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [presetInURL], async presetInURL => {
        let $ = content.document.querySelector.bind(content.document);
        Assert.ok(
          !$("#some-elements-unavailable").hidden,
          "If a preset is selected via URL, a warning should be displayed."
        );
        var inInputSorted = $("#log-modules").value.split(",").sort().join(",");
        var presetSorted = content
          .presets()
          [presetInURL].modules.split(",")
          .sort()
          .join(",");
        Assert.equal(
          inInputSorted,
          presetSorted,
          "When selecting a preset via URL params, the correct log modules are reflected in the input."
        );
      });
    }
  );
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE + "?profiler-preset=" + profilerPresetInURL,
    },
    async browser => {
      await SpecialPowers.spawn(browser, [profilerPresetInURL], async inURL => {
        let $ = content.document.querySelector.bind(content.document);
        // Threads override doesn't have a UI element, the warning shouldn't
        // be displayed.
        Assert.ok(
          $("#some-elements-unavailable").hidden,
          "When overriding the profiler preset, no warning is displayed on the page."
        );
        var inSettings = content.settings().profilerPreset;
        Assert.equal(
          inSettings,
          inURL,
          "When overriding the profiler preset via URL param, the correct preset is set in the logging manager settings."
        );
      });
    }
  );
  await BrowserTestUtils.withNewTab(PAGE + "?profilerstacks", async browser => {
    await SpecialPowers.spawn(browser, [], async () => {
      let $ = content.document.querySelector.bind(content.document);
      Assert.ok(
        !$("#some-elements-unavailable").hidden,
        "If the profiler stacks config is set via URL, a warning should be displayed."
      );
      Assert.ok(
        $("#with-profiler-stacks-checkbox").disabled,
        "If the profiler stacks config is set via URL, its checkbox should be disabled."
      );

      Assert.ok(
        Services.prefs.getBoolPref("logging.config.profilerstacks"),
        "The preference for profiler stacks is set initially, as a result of parsing the URL parameter"
      );

      $("#radio-logging-file").click();
      $("#radio-logging-profiler").click();

      Assert.ok(
        $("#with-profiler-stacks-checkbox").disabled,
        "If the profiler stacks config is set via URL, its checkbox should be disabled even after clicking around."
      );
    });
  });
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE + "?invalid-param",
    },
    async browser => {
      await SpecialPowers.spawn(browser, [profilerPresetInURL], async () => {
        let $ = content.document.querySelector.bind(content.document);
        Assert.ok(
          !$("#error").hidden,
          "When an invalid URL param is passed in, the page displays a warning."
        );
      });
    }
  );
  clearLoggingPrefs();
});

// Test various things related to presets: that it's populated correctly, that
// setting presets work in terms of UI, but also that it sets the logging.*
// prefs correctly.
add_task(async function testAboutLoggingPresets() {
  await BrowserTestUtils.withNewTab(PAGE, async browser => {
    await SpecialPowers.spawn(browser, [], async () => {
      let $ = content.document.querySelector.bind(content.document);
      let presetsDropdown = $("#logging-preset-dropdown");
      Assert.equal(
        Object.keys(content.presets()).length,
        presetsDropdown.childNodes.length,
        "Presets populated."
      );

      Assert.equal(presetsDropdown.value, "networking");
      $("#set-log-modules-button").click();
      Assert.ok(
        $("#no-log-modules").hidden && !$("#current-log-modules").hidden,
        "When log modules are set, they are visible."
      );
      var lengthModuleListNetworking = $("#log-modules").value.length;
      var lengthCurrentModuleListNetworking = $("#current-log-modules")
        .innerText.length;
      Assert.notEqual(
        lengthModuleListNetworking,
        0,
        "When setting a profiler preset, the module string is non-empty (input)."
      );
      Assert.notEqual(
        lengthCurrentModuleListNetworking,
        0,
        "When setting a profiler preset, the module string is non-empty (selected modules)."
      );

      // Change preset
      presetsDropdown.value = "media-playback";
      presetsDropdown.dispatchEvent(new content.Event("change"));

      // Check the following after "onchange".
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(resolve => content.setTimeout(resolve, 0));

      Assert.equal(
        presetsDropdown.value,
        "media-playback",
        "Selecting another preset is reflected in the page"
      );
      $("#set-log-modules-button").click();
      Assert.ok(
        $("#no-log-modules").hidden && !$("#current-log-modules").hidden,
        "When other log modules are set, they are still visible"
      );
      Assert.notEqual(
        $("#log-modules").value.length,
        0,
        "When setting a profiler preset, the module string is non-empty (input)."
      );
      Assert.notEqual(
        $("#current-log-modules").innerText.length,
        0,
        "When setting a profiler preset, the module string is non-empty (selected modules)."
      );
      Assert.notEqual(
        $("#log-modules").value.length,
        lengthModuleListNetworking,
        "When setting another profiler preset, the module string changes (input)."
      );
      let currentLogModulesString = $("#current-log-modules").innerText;
      Assert.notEqual(
        currentLogModulesString.length,
        lengthCurrentModuleListNetworking,

        "When setting another profiler preset, the module string changes (selected modules)."
      );

      // After setting some log modules via the preset dropdown, verify
      // that they have been reflected to logging.* preferences.
      var activeLogModules = [];
      let children = Services.prefs.getBranch("logging.").getChildList("");
      for (let pref of children) {
        if (pref.startsWith("config.")) {
          continue;
        }

        try {
          let value = Services.prefs.getIntPref(`logging.${pref}`);
          activeLogModules.push(`${pref}:${value}`);
        } catch (e) {
          console.error(e);
        }
      }
      let mod;
      while ((mod = activeLogModules.pop())) {
        Assert.ok(
          currentLogModulesString.includes(mod),
          `${mod} was effectively set`
        );
      }
    });
  });
  clearLoggingPrefs();
});

// Test various things around the profiler stacks feature
add_task(async function testProfilerStacks() {
  // Check the initial state before changing anything.
  Assert.ok(
    !Services.prefs.getBoolPref("logging.config.profilerstacks", false),
    "The preference for profiler stacks isn't set initially"
  );
  await BrowserTestUtils.withNewTab(PAGE, async browser => {
    await SpecialPowers.spawn(browser, [], async () => {
      let $ = content.document.querySelector.bind(content.document);
      const checkbox = $("#with-profiler-stacks-checkbox");
      Assert.ok(
        !checkbox.checked,
        "The profiler stacks checkbox isn't checked at load time."
      );
      checkbox.checked = true;
      checkbox.dispatchEvent(new content.Event("change"));
      Assert.ok(
        Services.prefs.getBoolPref("logging.config.profilerstacks"),
        "The preference for profiler stacks is now set to true"
      );
      checkbox.checked = false;
      checkbox.dispatchEvent(new content.Event("change"));
      Assert.ok(
        !Services.prefs.getBoolPref("logging.config.profilerstacks"),
        "The preference for profiler stacks is now back to false"
      );

      $("#radio-logging-file").click();
      Assert.ok(
        checkbox.disabled,
        "The profiler stacks checkbox is disabled when the output type is 'file'"
      );
      $("#radio-logging-profiler").click();
      Assert.ok(
        !checkbox.disabled,
        "The profiler stacks checkbox is enabled when the output type is 'profiler'"
      );
    });
  });
  clearLoggingPrefs();
});

// Here we test that starting and stopping log collection to the Firefox
// Profiler opens a new tab. We don't actually check the content of the profile.
add_task(async function testProfilerOpens() {
  await BrowserTestUtils.withNewTab(PAGE, async browser => {
    let profilerOpenedPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      "https://example.com/",
      false
    );
    SpecialPowers.spawn(browser, [], async () => {
      let $ = content.document.querySelector.bind(content.document);
      // Override the URL the profiler uses to avoid hitting external
      // resources (and crash).
      await SpecialPowers.pushPrefEnv({
        set: [
          ["devtools.performance.recording.ui-base-url", "https://example.com"],
          ["devtools.performance.recording.ui-base-url-path", "/"],
        ],
      });
      $("#radio-logging-file").click();
      $("#radio-logging-profiler").click();
      $("#logging-preset-dropdown").value = "networking";
      $("#logging-preset-dropdown").dispatchEvent(new content.Event("change"));
      $("#set-log-modules-button").click();
      $("#toggle-logging-button").click();
      // Wait for the profiler to start. This can be very slow.
      await content.profilerPromise();

      // Wait for some time for good measure while the profiler collects some
      // data. We don't really care about the data itself.
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(resolve => content.setTimeout(resolve, 1000));
      $("#toggle-logging-button").click();
    });
    let tab = await profilerOpenedPromise;
    Assert.ok(true, "Profiler tab opened after profiling");
    await BrowserTestUtils.removeTab(tab);
  });
  clearLoggingPrefs();
});

// Same test, outputing to a file, with network logging, while opening and
// closing a tab. We only check that the file exists and has a non-zero size.
add_task(async function testLogFileFound() {
  await BrowserTestUtils.withNewTab(PAGE, async browser => {
    await SpecialPowers.spawn(browser, [], async () => {
      // Clear any previous log file.
      let $ = content.document.querySelector.bind(content.document);
      $("#radio-logging-file").click();
      $("#log-file").value = "";
      $("#log-file").dispatchEvent(new content.Event("change"));
      $("#set-log-file-button").click();

      Assert.ok(
        !$("#no-log-file").hidden,
        "When a log file hasn't been set, it's indicated as such."
      );
    });
  });
  await BrowserTestUtils.withNewTab(PAGE, async browser => {
    let logPath = await SpecialPowers.spawn(browser, [], async () => {
      let $ = content.document.querySelector.bind(content.document);
      $("#radio-logging-file").click();
      // Set the log file (use the default path)
      $("#set-log-file-button").click();
      var logPath = $("#current-log-file").innerText;
      // Set log modules for networking
      $("#logging-preset-dropdown").value = "networking";
      $("#logging-preset-dropdown").dispatchEvent(new content.Event("change"));
      $("#set-log-modules-button").click();
      return logPath;
    });

    // No need to start or stop logging when logging to a file. Just open
    // a tab, any URL will do. Wait for this tab to be loaded so we're sure
    // something (anything) has happened in necko.
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "https://example.com",
      true /* waitForLoad */
    );
    await BrowserTestUtils.removeTab(tab);
    let logDirectory = PathUtils.parent(logPath);
    let logBasename = PathUtils.filename(logPath);
    let entries = await IOUtils.getChildren(logDirectory);
    let foundNonEmptyLogFile = false;
    for (let entry of entries) {
      if (entry.includes(logBasename)) {
        info("-- Log file found: " + entry);
        let fileinfo = await IOUtils.stat(entry);
        foundNonEmptyLogFile |= fileinfo.size > 0;
      }
    }
    Assert.ok(foundNonEmptyLogFile, "Found at least one non-empty log file.");
  });
  clearLoggingPrefs();
});
