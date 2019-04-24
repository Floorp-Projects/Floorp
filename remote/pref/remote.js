// Indicates whether the remote agent is enabled.
// If it is false, the remote agent will not be loaded.
pref("remote.enabled", false);

// Limits remote agent to listen on loopback devices,
// e.g. 127.0.0.1, localhost, and ::1.
pref("remote.force-local", true);

// Defines the verbosity of the internal logger.
//
// Available levels are, in descending order of severity,
// "Trace", "Debug", "Config", "Info", "Warn", "Error", and "Fatal".
// The value is treated case-sensitively.
pref("remote.log.level", "Info");
