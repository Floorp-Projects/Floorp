add_task(async function run_test() {
  if (!("@mozilla.org/toolkit/crash-reporter;1" in Cc)) {
    dump(
      "INFO | test_crash_modules.js | Can't test crashreporter in a non-libxul build.\n"
    );
    return;
  }

  await do_crash(
    function() {
      const { ctypes } = ChromeUtils.import(
        "resource://gre/modules/ctypes.jsm"
      );
      // Load and unload a DLL so that it will show up as unloaded in the minidump
      let lib = ctypes.open("wininet");
      lib.close();
    },
    async function(mdump, extra, extraFile) {
      runMinidumpAnalyzer(mdump);

      // Refresh updated extra data
      let data = await OS.File.read(extraFile.path);
      let decoder = new TextDecoder();
      extra = JSON.parse(decoder.decode(data));

      // Check unloaded modules
      const unloadedModules = extra.StackTraces.unloaded_modules;
      Assert.ok(!!unloadedModules, "The unloaded_modules field exists");
      Assert.notEqual(unloadedModules.find(e => e.filename == "wininet.DLL"));

      // Check the module signature information
      const sigInfo = JSON.parse(extra.ModuleSignatureInfo);
      Assert.ok(
        !!sigInfo["Microsoft Windows"],
        "The module signature info contains valid data"
      );
      Assert.greater(
        sigInfo["Microsoft Windows"].length,
        0,
        "Multiple signed binaries were found"
      );
    },
    // process will exit with a zero exit status
    true
  );
});
