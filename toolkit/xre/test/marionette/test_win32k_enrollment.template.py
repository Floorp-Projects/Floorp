from __future__ import absolute_import, print_function

from marionette_harness import MarionetteTestCase
from contextlib import contextmanager


class ExperimentStatus:
    UNENROLLED = 0
    ENROLLED_CONTROL = 1
    ENROLLED_TREATMENT = 2
    DISQUALIFIED = 3


class ContentWin32kLockdownState:
    LockdownEnabled = 1
    MissingWebRender = 2
    OperatingSystemNotSupported = 3
    PrefNotSet = 4
    MissingRemoteWebGL = 5
    MissingNonNativeTheming = 6
    DisabledByEnvVar = 7
    DisabledBySafeMode = 8
    DisabledByE10S = 9
    DisabledByUserPref = 10
    EnabledByUserPref = 11
    DisabledByControlGroup = 12
    EnabledByTreatmentGroup = 13
    DisabledByDefault = 14
    EnabledByDefault = 15


class Prefs:
    ENROLLMENT_STATUS = "security.sandbox.content.win32k-experiment.enrollmentStatus"
    STARTUP_ENROLLMENT_STATUS = (
        "security.sandbox.content.win32k-experiment.startupEnrollmentStatus"
    )
    WIN32K = "security.sandbox.content.win32k-disable"
    WEBGL = "webgl.out-of-process"


ENV_DISABLE_WIN32K = "MOZ_ENABLE_WIN32K"
ENV_DISABLE_E10S = "MOZ_FORCE_DISABLE_E10S"


class TestWin32kAutostart(MarionetteTestCase):
    SANDBOX_NAME = "win32k-autostart"

    def execute_script(self, code, *args, **kwargs):
        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            return self.marionette.execute_script(
                code, new_sandbox=False, sandbox=self.SANDBOX_NAME, *args, **kwargs
            )

    def get_win32k_status(self):
        return self.execute_script(
            r"""
          let win = Services.wm.getMostRecentWindow("navigator:browser");
          let ses = "security.sandbox.content.win32k-experiment.startupEnrollmentStatus";
          return {
            win32kSessionStatus: Services.appinfo.win32kSessionStatus,
            win32kStatus: Services.appinfo.win32kLiveStatusTestingOnly,
            win32kExperimentStatus: Services.appinfo.win32kExperimentStatus,
            win32kPref: Services.prefs.getBoolPref("security.sandbox.content.win32k-disable"),
            win32kStartupEnrollmentStatusPref: Services.prefs.getIntPref(ses),
          };
        """
        )

    def check_win32k_status(
        self, status, sessionStatus, experimentStatus, pref, enrollmentStatusPref
    ):
        # We CANNOT check win32kEnrollmentStatusPref after a restart because we only set this
        # pref on the default branch, and it goes away after a restart, so we only check
        # the startupEnrollmentStatusPref
        expected = {
            "win32kSessionStatus": sessionStatus,
            "win32kStatus": status,
            "win32kExperimentStatus": experimentStatus,
            "win32kPref": pref,
            "win32kStartupEnrollmentStatusPref": enrollmentStatusPref,
        }

        status = self.get_win32k_status()

        for prop, value in expected.items():
            self.assertEqual(
                status[prop],
                value,
                "%s should have the value `%r`, but has `%r`"
                % (prop, value, status[prop]),
            )

    def set_env(self, env, value):
        self.execute_script(
            "env.set(arguments[0], arguments[1]);", script_args=(env, value)
        )

    def get_env(self, env):
        return self.execute_script("return env.get(arguments[0]);", script_args=(env,))

    def set_enrollment_status(self, status):
        self.marionette.set_pref(Prefs.ENROLLMENT_STATUS, status, default_branch=True)

        updated_status = self.marionette.get_pref(Prefs.ENROLLMENT_STATUS)
        self.assertTrue(
            status == updated_status or updated_status == ExperimentStatus.DISQUALIFIED
        )
        startup_status = self.marionette.get_pref(Prefs.STARTUP_ENROLLMENT_STATUS)
        self.assertEqual(
            startup_status,
            updated_status,
            "Startup enrollment status (%r) should match "
            "session status (%r)" % (startup_status, updated_status),
        )

    def restart(self, prefs=None, env=None):
        if prefs:
            self.marionette.set_prefs(prefs)

        if env:
            for name, value in env.items():
                self.set_env(name, value)

        self.marionette.restart(in_app=True, clean=False)
        self.setUpSession()

        # Sanity check our environment.
        if prefs:
            for key, val in prefs.items():
                if val is not None:
                    self.assertEqual(self.marionette.get_pref(key), val)
        if env:
            for key, val in env.items():
                self.assertEqual(self.get_env(key), val or "")

    def setUpSession(self):
        self.marionette.set_context(self.marionette.CONTEXT_CHROME)

        self.execute_script(
            r"""
          // We're running in a function, in a sandbox, that inherits from an
          // X-ray wrapped window. Anything we want to be globally available
          // needs to be defined on that window.
          window.env = Cc["@mozilla.org/process/environment;1"]
                    .getService(Ci.nsIEnvironment);
        """
        )

    @contextmanager
    def full_restart(self):
        profile = self.marionette.instance.profile
        try:
            self.marionette.quit(in_app=True, clean=False)
            yield profile
        finally:
            self.marionette.start_session()
            self.setUpSession()

    def setUp(self):
        super(TestWin32kAutostart, self).setUp()

        # If we have configured marionette to require a particular value for
        # `win32k.autostart`, remove it as a forced pref until `tearDown`, and
        # perform a clean restart, so we run this test without the pref
        # pre-configured.
        self.win32kRequired = None
        if Prefs.WIN32K in self.marionette.instance.required_prefs:
            self.win32kRequired = self.marionette.instance.required_prefs[Prefs.WIN32K]
            del self.marionette.instance.required_prefs[Prefs.WIN32K]
            self.marionette.restart(clean=True)

        self.setUpSession()

        # Marionette doesn't let you set preferences on the default branch before startup
        # so we can't test the default=False and default=True scenarios in one test
        # What we can do is generate all the tests, and then only run the runs for which
        # the default is.  (And run the other ones locally to make sure they work before
        # we land it.)
        prefJS = 'return Services.prefs.getBoolPref("security.sandbox.content.win32k-disable");'
        self.default_is = self.execute_script(prefJS)

        if self.default_is is False:
            # Win32k status must start out with `disabledByDefault`
            self.check_win32k_status(
                status=ContentWin32kLockdownState.DisabledByDefault,
                sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
                experimentStatus=ExperimentStatus.UNENROLLED,
                pref=False,
                enrollmentStatusPref=ExperimentStatus.UNENROLLED,
            )
        else:
            # Win32k status must start out with `enabledByDefault`
            self.check_win32k_status(
                status=ContentWin32kLockdownState.EnabledByDefault,
                sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
                experimentStatus=ExperimentStatus.UNENROLLED,
                pref=True,
                enrollmentStatusPref=ExperimentStatus.UNENROLLED,
            )

    def tearDown(self):
        if self.win32kRequired is not None:
            self.marionette.instance.required_prefs[Prefs.WIN32K] = self.win32kRequired
        self.marionette.restart(clean=True)

        super(TestWin32kAutostart, self).tearDown()
