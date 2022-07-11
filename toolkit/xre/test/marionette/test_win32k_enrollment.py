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

    def test_1(self):
        # [D=F] Nothing [A#1] -> Enrolled Control [A S=DisabledByDefault SS=DisabledByDefa...

        if self.default_is is not False:
            return

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

    def test_2(self):
        # [D=F] Nothing [A#1] -> Enrolled Treatment [A S=DisabledByDefault SS=DisabledByDe...

        if self.default_is is not False:
            return

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

    def test_3(self):
        # [D=F] Nothing [A#1] -> On [A S=EnabledByUserPref SS=DisabledByDefault ES=UNENROL...

        if self.default_is is not False:
            return

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByUserPref,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_4(self):
        # [D=F] Nothing [A#1] -> Off [A#1] -> Restart [A#1]...

        if self.default_is is not False:
            return

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WIN32K, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_5(self):
        # [D=F] Nothing [A#1] -> On -> Bad Requirements [A S=MissingRemoteWebGL SS=Disable...

        if self.default_is is not False:
            return

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.marionette.set_pref(Prefs.WEBGL, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_6(self):
        # [D=F] Nothing [A#1] -> On -> E10S [A S=DisabledByE10S SS=DisabledByE10S ES=UNENR...

        if self.default_is is not False:
            return

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WIN32K, True)

        app_version = self.execute_script("return Services.appinfo.version")
        self.restart(env={ENV_DISABLE_E10S: app_version})
        self.set_env(ENV_DISABLE_E10S, "null")

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByE10S,
            sessionStatus=ContentWin32kLockdownState.DisabledByE10S,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_7(self):
        # [D=F] Nothing [A#1] -> On -> Header-On [A S=DisabledByEnvVar SS=DisabledByEnvVar...

        if self.default_is is not False:
            return

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.restart(env={ENV_DISABLE_WIN32K: "1"})

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByEnvVar,
            sessionStatus=ContentWin32kLockdownState.DisabledByEnvVar,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.set_env(ENV_DISABLE_WIN32K, "")

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.DisabledByEnvVar,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_8(self):
        # [D=T] Nothing [A#1T] -> Enrolled Control [A S=EnabledByDefault SS=EnabledByDefau...

        if self.default_is is not True:
            return

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

    def test_9(self):
        # [D=T] Nothing [A#1T] -> Enrolled Treatment [A S=EnabledByDefault SS=EnabledByDef...

        if self.default_is is not True:
            return

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

    def test_10(self):
        # [D=T] Nothing [A#1T] -> On [A S=EnabledByDefault SS=EnabledByDefault ES=UNENROLL...

        if self.default_is is not True:
            return

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_11(self):
        # [D=T] Nothing [A#1T] -> Off [A S=DisabledByUserPref SS=EnabledByDefault ES=UNENR...

        if self.default_is is not True:
            return

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WIN32K, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.DisabledByUserPref,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_12(self):
        # [D=T] Nothing [A#1T] -> On -> Bad Requirements [A S=MissingRemoteWebGL SS=Enable...

        if self.default_is is not True:
            return

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.marionette.set_pref(Prefs.WEBGL, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_13(self):
        # [D=T] Nothing [A#1T] -> On -> E10S [A S=DisabledByE10S SS=DisabledByE10S ES=UNEN...

        if self.default_is is not True:
            return

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WIN32K, True)

        app_version = self.execute_script("return Services.appinfo.version")
        self.restart(env={ENV_DISABLE_E10S: app_version})
        self.set_env(ENV_DISABLE_E10S, "null")

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByE10S,
            sessionStatus=ContentWin32kLockdownState.DisabledByE10S,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_14(self):
        # [D=T] Nothing [A#1T] -> On -> Header-On [A S=DisabledByEnvVar SS=DisabledByEnvVa...

        if self.default_is is not True:
            return

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.restart(env={ENV_DISABLE_WIN32K: "1"})

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByEnvVar,
            sessionStatus=ContentWin32kLockdownState.DisabledByEnvVar,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.set_env(ENV_DISABLE_WIN32K, "")

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByEnvVar,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_15(self):
        # [D=F] On [A#3] -> Restart [A#4] -> Enrolled Control [A S=EnabledByUserPref SS=En...

        if self.default_is is not False:
            return

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByUserPref,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByUserPref,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByUserPref,
            experimentStatus=ExperimentStatus.DISQUALIFIED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.DISQUALIFIED,
        )

    def test_16(self):
        # [D=F] On [A#3] -> Restart [A#4] -> Enrolled Treatment [A S=EnabledByUserPref SS=...

        if self.default_is is not False:
            return

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByUserPref,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByUserPref,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByUserPref,
            experimentStatus=ExperimentStatus.DISQUALIFIED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.DISQUALIFIED,
        )

    def test_17(self):
        # [D=F] On [A#3] -> Restart [A#4] -> Off [A S=DisabledByDefault SS=EnabledByUserPr...

        if self.default_is is not False:
            return

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByUserPref,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WIN32K, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByUserPref,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_18(self):
        # [D=F] On [A#3] -> Restart [A#4] -> Bad Requirements [A S=MissingRemoteWebGL SS=E...

        if self.default_is is not False:
            return

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByUserPref,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WEBGL, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.EnabledByUserPref,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_19(self):
        # [D=F] On [A#3] -> Restart [A#4] -> E10S [A S=DisabledByE10S SS=DisabledByE10S ES...

        if self.default_is is not False:
            return

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByUserPref,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        app_version = self.execute_script("return Services.appinfo.version")
        self.restart(env={ENV_DISABLE_E10S: app_version})
        self.set_env(ENV_DISABLE_E10S, "null")

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByE10S,
            sessionStatus=ContentWin32kLockdownState.DisabledByE10S,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByUserPref,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_20(self):
        # [D=F] On [A#3] -> Restart [A#4] -> Header-On [A S=DisabledByEnvVar SS=DisabledBy...

        if self.default_is is not False:
            return

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByUserPref,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart(env={ENV_DISABLE_WIN32K: "1"})

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByEnvVar,
            sessionStatus=ContentWin32kLockdownState.DisabledByEnvVar,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.set_env(ENV_DISABLE_WIN32K, "")

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByUserPref,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_21(self):
        # [D=T] On [A#3T] -> Restart [A#4T] -> Enrolled Control [A S=EnabledByDefault SS=E...

        if self.default_is is not True:
            return

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

    def test_22(self):
        # [D=T] On [A#3T] -> Restart [A#4T] -> Enrolled Treatment [A S=EnabledByDefault SS...

        if self.default_is is not True:
            return

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

    def test_23(self):
        # [D=T] On [A#3T] -> Restart [A#4T] -> Off [A S=DisabledByUserPref SS=EnabledByDef...

        if self.default_is is not True:
            return

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WIN32K, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.DisabledByUserPref,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_24(self):
        # [D=T] On [A#3T] -> Restart [A#4T] -> Bad Requirements [A S=MissingRemoteWebGL SS...

        if self.default_is is not True:
            return

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WEBGL, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_25(self):
        # [D=T] On [A#3T] -> Restart [A#4T] -> E10S [A S=DisabledByE10S SS=DisabledByE10S ...

        if self.default_is is not True:
            return

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        app_version = self.execute_script("return Services.appinfo.version")
        self.restart(env={ENV_DISABLE_E10S: app_version})
        self.set_env(ENV_DISABLE_E10S, "null")

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByE10S,
            sessionStatus=ContentWin32kLockdownState.DisabledByE10S,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_26(self):
        # [D=T] On [A#3T] -> Restart [A#4T] -> Header-On [A S=DisabledByEnvVar SS=Disabled...

        if self.default_is is not True:
            return

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart(env={ENV_DISABLE_WIN32K: "1"})

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByEnvVar,
            sessionStatus=ContentWin32kLockdownState.DisabledByEnvVar,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.set_env(ENV_DISABLE_WIN32K, "")

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_27(self):
        # [D=F] Enrolled Control [A#5] -> Restart [A#6] -> Enrolled Control-C -> On [A S=E...

        if self.default_is is not False:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.DISQUALIFIED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByUserPref,
            experimentStatus=ExperimentStatus.DISQUALIFIED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.DISQUALIFIED,
        )

    def test_28(self):
        # [D=F] Enrolled Control [A#5] -> Restart [A#6] -> Off [A#6]...

        if self.default_is is not False:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.marionette.set_pref(Prefs.WIN32K, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

    def test_29(self):
        # [D=F] Enrolled Control [A#5] -> Restart [A#6] -> Enrolled Control-C -> Bad Requi...

        if self.default_is is not False:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.marionette.set_pref(Prefs.WEBGL, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.DISQUALIFIED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.DISQUALIFIED,
        )

    def test_30(self):
        # [D=F] Enrolled Control [A#5] -> Restart [A#6] -> Enrolled Control-C -> E10S [A S...

        if self.default_is is not False:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        app_version = self.execute_script("return Services.appinfo.version")
        self.restart(env={ENV_DISABLE_E10S: app_version})
        self.set_env(ENV_DISABLE_E10S, "null")

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByE10S,
            sessionStatus=ContentWin32kLockdownState.DisabledByE10S,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

    def test_31(self):
        # [D=F] Enrolled Control [A#5] -> Restart [A#6] -> Enrolled Control-C -> Header-On...

        if self.default_is is not False:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.restart(env={ENV_DISABLE_WIN32K: "1"})

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByEnvVar,
            sessionStatus=ContentWin32kLockdownState.DisabledByEnvVar,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.set_env(ENV_DISABLE_WIN32K, "")

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

    def test_32(self):
        # [D=T] Enrolled Control [A#5T] -> Restart [A#6T] -> Enrolled Control-C -> On [A S...

        if self.default_is is not True:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

    def test_33(self):
        # [D=T] Enrolled Control [A#5T] -> Restart [A#6T] -> Off [A S=DisabledByUserPref S...

        if self.default_is is not True:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.marionette.set_pref(Prefs.WIN32K, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.DISQUALIFIED,
        )

    def test_34(self):
        # [D=T] Enrolled Control [A#5T] -> Restart [A#6T] -> Enrolled Control-C -> Bad Req...

        if self.default_is is not True:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.marionette.set_pref(Prefs.WEBGL, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.DISQUALIFIED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.DISQUALIFIED,
        )

    def test_35(self):
        # [D=T] Enrolled Control [A#5T] -> Restart [A#6T] -> Enrolled Control-C -> E10S [A...

        if self.default_is is not True:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        app_version = self.execute_script("return Services.appinfo.version")
        self.restart(env={ENV_DISABLE_E10S: app_version})
        self.set_env(ENV_DISABLE_E10S, "null")

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByE10S,
            sessionStatus=ContentWin32kLockdownState.DisabledByE10S,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

    def test_36(self):
        # [D=T] Enrolled Control [A#5T] -> Restart [A#6T] -> Enrolled Control-C -> Header-...

        if self.default_is is not True:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.restart(env={ENV_DISABLE_WIN32K: "1"})

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByEnvVar,
            sessionStatus=ContentWin32kLockdownState.DisabledByEnvVar,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.set_env(ENV_DISABLE_WIN32K, "")

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByControlGroup,
            sessionStatus=ContentWin32kLockdownState.DisabledByControlGroup,
            experimentStatus=ExperimentStatus.ENROLLED_CONTROL,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

    def test_37(self):
        # [D=F] Enrolled Treatment [A#7] -> Restart [A#8] -> Enrolled Treatment-C -> On [A...

        if self.default_is is not False:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.DISQUALIFIED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByUserPref,
            experimentStatus=ExperimentStatus.DISQUALIFIED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.DISQUALIFIED,
        )

    def test_38(self):
        # [D=F] Enrolled Treatment [A#7] -> Restart [A#8] -> Enrolled Treatment-C -> Off [...

        if self.default_is is not False:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.marionette.set_pref(Prefs.WIN32K, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

    def test_39(self):
        # [D=F] Enrolled Treatment [A#7] -> Restart [A#8] -> Enrolled Treatment-C -> Bad R...

        if self.default_is is not False:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.marionette.set_pref(Prefs.WEBGL, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.DISQUALIFIED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.DISQUALIFIED,
        )

    def test_40(self):
        # [D=F] Enrolled Treatment [A#7] -> Restart [A#8] -> Enrolled Treatment-C -> E10S ...

        if self.default_is is not False:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        app_version = self.execute_script("return Services.appinfo.version")
        self.restart(env={ENV_DISABLE_E10S: app_version})
        self.set_env(ENV_DISABLE_E10S, "null")

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByE10S,
            sessionStatus=ContentWin32kLockdownState.DisabledByE10S,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

    def test_41(self):
        # [D=F] Enrolled Treatment [A#7] -> Restart [A#8] -> Enrolled Treatment-C -> Heade...

        if self.default_is is not False:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByDefault,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.restart(env={ENV_DISABLE_WIN32K: "1"})

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByEnvVar,
            sessionStatus=ContentWin32kLockdownState.DisabledByEnvVar,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.set_env(ENV_DISABLE_WIN32K, "")

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

    def test_42(self):
        # [D=T] Enrolled Treatment [A#7T] -> Restart [A#8T] -> Enrolled Treatment-C -> On ...

        if self.default_is is not True:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

    def test_43(self):
        # [D=T] Enrolled Treatment [A#7T] -> Restart [A#8T] -> Enrolled Treatment-C -> Off...

        if self.default_is is not True:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.marionette.set_pref(Prefs.WIN32K, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.DISQUALIFIED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByUserPref,
            sessionStatus=ContentWin32kLockdownState.DisabledByUserPref,
            experimentStatus=ExperimentStatus.DISQUALIFIED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.DISQUALIFIED,
        )

    def test_44(self):
        # [D=T] Enrolled Treatment [A#7T] -> Restart [A#8T] -> Enrolled Treatment-C -> Bad...

        if self.default_is is not True:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.marionette.set_pref(Prefs.WEBGL, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.DISQUALIFIED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.DISQUALIFIED,
        )

    def test_45(self):
        # [D=T] Enrolled Treatment [A#7T] -> Restart [A#8T] -> Enrolled Treatment-C -> E10...

        if self.default_is is not True:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        app_version = self.execute_script("return Services.appinfo.version")
        self.restart(env={ENV_DISABLE_E10S: app_version})
        self.set_env(ENV_DISABLE_E10S, "null")

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByE10S,
            sessionStatus=ContentWin32kLockdownState.DisabledByE10S,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

    def test_46(self):
        # [D=T] Enrolled Treatment [A#7T] -> Restart [A#8T] -> Enrolled Treatment-C -> Hea...

        if self.default_is is not True:
            return
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByDefault,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.restart(env={ENV_DISABLE_WIN32K: "1"})

        self.check_win32k_status(
            status=ContentWin32kLockdownState.DisabledByEnvVar,
            sessionStatus=ContentWin32kLockdownState.DisabledByEnvVar,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.set_env(ENV_DISABLE_WIN32K, "")

        # Re-set enrollment pref, like Normandy would do
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            sessionStatus=ContentWin32kLockdownState.EnabledByTreatmentGroup,
            experimentStatus=ExperimentStatus.ENROLLED_TREATMENT,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

    def test_47(self):
        # [D=F] Bad Requirements [A#9] -> Restart [A#10] -> Enrolled Control [A S=MissingR...

        if self.default_is is not False:
            return

        self.marionette.set_pref(Prefs.WEBGL, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.DISQUALIFIED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.DISQUALIFIED,
        )

    def test_48(self):
        # [D=F] Bad Requirements [A#9] -> Restart [A#10] -> Enrolled Treatment [A S=Missin...

        if self.default_is is not False:
            return

        self.marionette.set_pref(Prefs.WEBGL, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.DISQUALIFIED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.DISQUALIFIED,
        )

    def test_49(self):
        # [D=F] Bad Requirements [A#9] -> Restart [A#10] -> On [A S=MissingRemoteWebGL SS=...

        if self.default_is is not False:
            return

        self.marionette.set_pref(Prefs.WEBGL, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_50(self):
        # [D=F] Bad Requirements [A#9] -> Restart [A#10] -> Off [A S=MissingRemoteWebGL SS...

        if self.default_is is not False:
            return

        self.marionette.set_pref(Prefs.WEBGL, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.DisabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WIN32K, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_51(self):
        # [D=T] Bad Requirements [A#9T] -> Restart [A#10T] -> Enrolled Control [A S=Missin...

        if self.default_is is not True:
            return

        self.marionette.set_pref(Prefs.WEBGL, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )
        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_CONTROL,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.DISQUALIFIED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.DISQUALIFIED,
        )

    def test_52(self):
        # [D=T] Bad Requirements [A#9T] -> Restart [A#10T] -> Enrolled Treatment [A S=Miss...

        if self.default_is is not True:
            return

        self.marionette.set_pref(Prefs.WEBGL, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )
        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.ENROLLED_TREATMENT,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.DISQUALIFIED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.DISQUALIFIED,
        )

    def test_53(self):
        # [D=T] Bad Requirements [A#9T] -> Restart [A#10T] -> On [A S=MissingRemoteWebGL S...

        if self.default_is is not True:
            return

        self.marionette.set_pref(Prefs.WEBGL, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WIN32K, True)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

    def test_54(self):
        # [D=T] Bad Requirements [A#9T] -> Restart [A#10T] -> Off [A S=MissingRemoteWebGL ...

        if self.default_is is not True:
            return

        self.marionette.set_pref(Prefs.WEBGL, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.EnabledByDefault,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.restart()

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=True,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )

        self.marionette.set_pref(Prefs.WIN32K, False)

        self.check_win32k_status(
            status=ContentWin32kLockdownState.MissingRemoteWebGL,
            sessionStatus=ContentWin32kLockdownState.MissingRemoteWebGL,
            experimentStatus=ExperimentStatus.UNENROLLED,
            pref=False,
            enrollmentStatusPref=ExperimentStatus.UNENROLLED,
        )
