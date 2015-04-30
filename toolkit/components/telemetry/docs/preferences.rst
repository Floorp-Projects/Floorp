Preferences
===========

Telemetry behaviour is controlled through the following preferences:

``toolkit.telemetry.unified``

  This controls whether unified behavior is enabled. If true:

  * Telemetry is always enabled and recording *base* data.
  * Telemetry will send additional ``main`` pings.

``toolkit.telemetry.enabled``

  If unified is off, this controls whether the Telemetry module is enabled.
  If unified is on, this controls whether to record *extended* data.
  This preference is controlled through the `Preferences` dialog.
