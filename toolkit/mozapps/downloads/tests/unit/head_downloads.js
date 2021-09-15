var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

registerCleanupFunction(function() {
  Services.startup.advanceShutdownPhase(
    Services.startup.SHUTDOWN_PHASE_APPSHUTDOWNCONFIRMED
  );
});
