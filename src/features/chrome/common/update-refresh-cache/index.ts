export function init() {
  
  // if (
  //   Services.prefs.getStringPref("noraneko.version2", "") !==
  //   NoranekoConstants.version2
  // ) {
  //   console.debug(
  //     "[noraneko updater] pref 'noraneko.version2' !== NoranekoConstants.version2",
  //   );

  //   //set version2
  //   if (Services.prefs.prefHasDefaultValue("noraneko.version2")) {
  //     Services.prefs.unlockPref("noraneko.version2");
  //   }

  //   Services.prefs
  //     .getDefaultBranch(null as unknown as string)
  //     .setStringPref("noraneko.version2", NoranekoConstants.version2);
  //   Services.prefs.lockPref("noraneko.version2");

  //   Services.prefs.savePrefFile(null);

  //   //invalidate cache and restart
  //   Services.appinfo.invalidateCachesOnRestart();
  //   // Services.startup.quit(
  //   //   Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eAttemptQuit,
  //   // );
  // }
}
