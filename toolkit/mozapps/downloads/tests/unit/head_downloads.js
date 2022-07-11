registerCleanupFunction(function() {
  Services.startup.advanceShutdownPhase(
    Services.startup.SHUTDOWN_PHASE_APPSHUTDOWNCONFIRMED
  );
});
