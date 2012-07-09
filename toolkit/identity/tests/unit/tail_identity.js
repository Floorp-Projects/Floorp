
// pre-emptively shut down to clear resources
if (typeof IdentityService !== "undefined") {
  IdentityService.shutdown();
} else if (typeof IDService !== "undefined") {
  IDService.shutdown();
}

