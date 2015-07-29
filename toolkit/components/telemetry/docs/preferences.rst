Preferences
===========

Telemetry behaviour is controlled through the preferences listed here.

Default behaviors
-----------------

On official builds (which define ``MOZILLA_OFFICIAL``), Telemetry is only initialized when ``MOZ_TELEMETRY_REPORTING`` is defined.
Sending only happens on official builds with ``MOZ_TELEMETRY_REPORTING`` defined.

Preferences
-----------

``toolkit.telemetry.unified``

  This controls whether unified behavior is enabled. If true:

  * Telemetry is always enabled and recording *base* data.
  * Telemetry will send additional ``main`` pings.

``toolkit.telemetry.unifiedIsOptIn``

  When true, we enable the Telemetry system only for people that opted into Telemetry, even if unified Telemetry is enabled.
  Defaults to false & requires a restart.

``toolkit.telemetry.enabled``

  If ``unified`` is off, this controls whether the Telemetry module is enabled.
  If ``unified`` is on, this controls whether to record *extended* data.
  This preference is controlled through the `Preferences` dialog.

  Note that the default value here of this pref depends on the define ``RELEASE_BUILD`` and the channel.
  If ``RELEASE_BUILD`` is set, ``MOZ_TELEMETRY_ON_BY_DEFAULT`` gets set, which means this pref will default to ``true``.
  This is overridden by the preferences code on the "beta" channel, the pref also defaults to ``true`` there.

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

Data-choices notification
-------------------------

``toolkit.telemetry.reportingpolicy.firstRun``

  This preference is not present until the first run. After, its value is set to false. This is used to show the infobar with a more aggressive timeout if it wasn't shown yet.

``datareporting.policy.dataSubmissionEnabled``

  This is the data submission master kill switch. If disabled, no policy is shown or upload takes place, ever.

``datareporting.policy.dataSubmissionPolicyNotifiedTime``

  Records the date user was shown the policy. This preference is also used on Android.

``datareporting.policy.dataSubmissionPolicyAcceptedVersion``

  Records the version of the policy notified to the user. This preference is also used on Android.

``datareporting.policy.dataSubmissionPolicyBypassNotification``

  Used in tests, it allows to skip the notification check.

``datareporting.policy.currentPolicyVersion``

  Stores the current policy version, overrides the default value defined in TelemetryReportingPolicy.jsm.

``datareporting.policy.minimumPolicyVersion``

  The minimum policy version that is accepted for the current policy. This can be set per channel.

``datareporting.policy.minimumPolicyVersion.channel-NAME``

  This is the only channel-specific version that we currently use for the minimum policy version.
