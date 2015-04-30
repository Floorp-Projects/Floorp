Preferences
===========

Telemetry behaviour is controlled through the preferences listed here.

*Note:* On official builds (which define ``MOZILLA_OFFICIAL``), Telemetry is only initialized when ``MOZ_TELEMETRY_REPORTING`` is defined.
Sending only happens on official builds with ``MOZ_TELEMETRY_REPORTING`` defined.

``toolkit.telemetry.unified``

  This controls whether unified behavior is enabled. If true:

  * Telemetry is always enabled and recording *base* data.
  * Telemetry will send additional ``main`` pings.

``toolkit.telemetry.enabled``

  If ``unified`` is off, this controls whether the Telemetry module is enabled.
  If ``unified`` is on, this controls whether to record *extended* data.
  This preference is controlled through the `Preferences` dialog.

``datareporting.healthreport.uploadEnabled``

  Send the data we record if user has consented to FHR. This preference is controlled through the `Preferences` dialog.

``toolkit.telemetry.archive.enabled``

  Allow pings to be archived locally. This can only be enabled if ``unified`` is on.

``toolkit.telemetry.server``

  The server Telemetry pings are sent to.

``toolkit.telemetry.log.level``

  This sets the Telemetry logging verbosity per ``Log.jsm``, with ``Trace`` or ``0`` being the most verbose and the default being ``Warn``.
  By default logging goes only the console service.

``toolkit.telemetry.log.dump``

  Sets whether to dump Telemetry log messages to ``stdout`` too.
