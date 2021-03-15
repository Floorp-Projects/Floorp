from __future__ import absolute_import, print_function

from marionette_harness import MarionetteTestCase
from contextlib import contextmanager


class ExperimentStatus:
    UNENROLLED = 0
    ENROLLED_CONTROL = 1
    ENROLLED_TREATMENT = 2
    DISQUALIFIED = 3


class Prefs:
    ENROLLMENT_STATUS = "fission.experiment.enrollmentStatus"
    STARTUP_ENROLLMENT_STATUS = "fission.experiment.startupEnrollmentStatus"
    FISSION_AUTOSTART = "fission.autostart"
    FISSION_AUTOSTART_SESSION = "fission.autostart.session"


ENV_ENABLE_FISSION = "MOZ_FORCE_ENABLE_FISSION"
ENV_DISABLE_E10S = "MOZ_FORCE_DISABLE_E10S"


DECISION_STATUS = {
    "experimentControl": 1,
    "experimentTreatment": 2,
    "disabledByE10sEnv": 3,
    "enabledByEnv": 4,
    "disabledBySafeMode": 5,
    "enabledByDefault": 6,
    "disabledByDefault": 7,
    "enabledByUserPref": 8,
    "disabledByUserPref": 9,
    "disabledByE10sOther": 10,
}


class TestFissionAutostart(MarionetteTestCase):
    SANDBOX_NAME = "fission-autostart"

    def execute_script(self, code, *args, **kwargs):
        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            return self.marionette.execute_script(
                code, new_sandbox=False, sandbox=self.SANDBOX_NAME, *args, **kwargs
            )

    def get_fission_status(self):
        return self.execute_script(
            r"""
          let win = Services.wm.getMostRecentWindow("navigator:browser");
          return {
            fissionAutostart: Services.appinfo.fissionAutostart,
            fissionExperimentStatus: Services.appinfo.fissionExperimentStatus,
            decisionStatus: Services.appinfo.fissionDecisionStatus,
            decisionStatusString: Services.appinfo.fissionDecisionStatusString,
            useRemoteSubframes: win.docShell.nsILoadContext.useRemoteSubframes,
            fissionAutostartSession: Services.prefs.getBoolPref("fission.autostart.session"),
            dynamicFissionAutostart: Services.prefs.getBoolPref("fission.autostart"),
            nonNativeTheme: Services.prefs.getBoolPref("widget.non-native-theme.enabled"),
          };
        """
        )

    def check_fission_status(self, enabled, experiment, decision, dynamic=None):
        if dynamic is None:
            dynamic = enabled

        expected = {
            "fissionAutostart": enabled,
            "fissionExperimentStatus": experiment,
            "decisionStatus": DECISION_STATUS[decision],
            "decisionStatusString": decision,
            "useRemoteSubframes": enabled,
            "fissionAutostartSession": enabled,
            "dynamicFissionAutostart": dynamic,
        }

        if expected["fissionExperimentStatus"] in (
            ExperimentStatus.ENROLLED_CONTROL,
            ExperimentStatus.ENROLLED_TREATMENT,
        ):
            expected["nonNativeTheme"] = True
        else:
            expected["nonNativeTheme"] = self.nightly_build

        status = self.get_fission_status()

        for prop, value in expected.items():
            self.assertEqual(
                status[prop],
                value,
                "%s should have the value `%r`, but has `%r`"
                % (prop, value, status[prop]),
            )

    def check_pref_locked(self):
        PREF = Prefs.FISSION_AUTOSTART

        if PREF in self.marionette.instance.required_prefs:
            return True

        res = self.execute_script(
            r"""
          const { AppConstants } = ChromeUtils.import(
            "resource://gre/modules/AppConstants.jsm"
          );
          return {
            prefLocked: Services.prefs.prefIsLocked(arguments[0]),
            releaseOrBeta: AppConstants.RELEASE_OR_BETA,
            nightlyBuild: AppConstants.NIGHTLY_BUILD,
          };
        """,
            script_args=(PREF,),
        )

        self.nightly_build = res["nightlyBuild"]

        if res["prefLocked"]:
            self.assertTrue(
                res["releaseOrBeta"], "Preference should only be locked on release/beta"
            )
            return True
        return False

    def set_env(self, env, value):
        self.execute_script(
            "env.set(arguments[0], arguments[1]);", script_args=(env, value)
        )

    def get_env(self, env):
        return self.execute_script("return env.get(arguments[0]);", script_args=(env,))

    def set_enrollment_status(self, status):
        self.marionette.set_pref(Prefs.ENROLLMENT_STATUS, status, default_branch=True)

        startup_status = self.marionette.get_pref(Prefs.STARTUP_ENROLLMENT_STATUS)
        self.assertEqual(
            startup_status,
            status,
            "Startup enrollment status (%r) should match new "
            "session status (%r)" % (startup_status, status),
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
          ChromeUtils.import("resource://gre/modules/Services.jsm", window);
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
        super(TestFissionAutostart, self).setUp()

        self.setUpSession()

    def tearDown(self):
        self.marionette.restart(clean=True)

        super(TestFissionAutostart, self).tearDown()

    def test_runtime_changes(self):
        """Tests that changes to preferences during runtime do not have any
        effect on the current session."""

        if self.check_pref_locked():
            # Need to be able to flip Fission prefs for this test to work.
            return

        self.check_fission_status(
            enabled=False,
            experiment=ExperimentStatus.UNENROLLED,
            decision="disabledByDefault",
        )

        self.restart(prefs={Prefs.FISSION_AUTOSTART: True})

        self.check_fission_status(
            enabled=True,
            experiment=ExperimentStatus.UNENROLLED,
            decision="enabledByUserPref",
        )

        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)
        self.check_fission_status(
            enabled=True,
            experiment=ExperimentStatus.UNENROLLED,
            decision="enabledByUserPref",
        )

        self.marionette.set_pref(Prefs.FISSION_AUTOSTART, False)
        self.check_fission_status(
            enabled=True,
            experiment=ExperimentStatus.UNENROLLED,
            decision="enabledByUserPref",
            dynamic=False,
        )

        self.marionette.clear_pref(Prefs.FISSION_AUTOSTART)
        self.check_fission_status(
            enabled=True,
            experiment=ExperimentStatus.UNENROLLED,
            decision="enabledByUserPref",
            dynamic=False,
        )

        self.restart()
        self.check_fission_status(
            enabled=False,
            experiment=ExperimentStatus.ENROLLED_CONTROL,
            decision="experimentControl",
        )

        self.marionette.set_pref(
            Prefs.ENROLLMENT_STATUS, ExperimentStatus.UNENROLLED, default_branch=True
        )
        self.check_fission_status(
            enabled=False,
            experiment=ExperimentStatus.ENROLLED_CONTROL,
            decision="experimentControl",
        )

        self.set_env(ENV_ENABLE_FISSION, "1")
        self.check_fission_status(
            enabled=False,
            experiment=ExperimentStatus.ENROLLED_CONTROL,
            decision="experimentControl",
        )

    def test_fission_precedence(self):
        if self.check_pref_locked():
            # Need to be able to flip Fission prefs for this test to work.
            return

        self.check_fission_status(
            enabled=False,
            experiment=ExperimentStatus.UNENROLLED,
            decision="disabledByDefault",
        )

        self.restart(
            prefs={Prefs.FISSION_AUTOSTART: False}, env={ENV_ENABLE_FISSION: "1"}
        )
        self.check_fission_status(
            enabled=True,
            experiment=ExperimentStatus.UNENROLLED,
            decision="enabledByEnv",
            dynamic=False,
        )

        self.restart(
            prefs={Prefs.FISSION_AUTOSTART: True}, env={ENV_ENABLE_FISSION: ""}
        )
        self.check_fission_status(
            enabled=True,
            experiment=ExperimentStatus.UNENROLLED,
            decision="enabledByUserPref",
        )

        self.restart(prefs={Prefs.FISSION_AUTOSTART: None})
        self.check_fission_status(
            enabled=False,
            experiment=ExperimentStatus.UNENROLLED,
            decision="disabledByDefault",
        )

        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)
        self.restart()
        self.check_fission_status(
            enabled=True,
            experiment=ExperimentStatus.ENROLLED_TREATMENT,
            decision="experimentTreatment",
        )

        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)
        self.restart()
        self.check_fission_status(
            enabled=False,
            experiment=ExperimentStatus.ENROLLED_CONTROL,
            decision="experimentControl",
        )

        self.marionette.set_pref(Prefs.FISSION_AUTOSTART, True)
        self.check_fission_status(
            enabled=False,
            experiment=ExperimentStatus.ENROLLED_CONTROL,
            decision="experimentControl",
            dynamic=True,
        )

        self.assertEqual(
            self.marionette.get_pref(Prefs.ENROLLMENT_STATUS),
            ExperimentStatus.DISQUALIFIED,
            "Setting fission.autostart should disqualify",
        )

        self.restart()
        self.check_fission_status(
            enabled=True,
            experiment=ExperimentStatus.DISQUALIFIED,
            decision="enabledByUserPref",
        )

        app_version = self.execute_script("return Services.appinfo.version")
        self.restart(env={ENV_DISABLE_E10S: app_version})
        self.check_fission_status(
            enabled=False,
            experiment=ExperimentStatus.DISQUALIFIED,
            decision="disabledByE10sEnv",
            dynamic=True,
        )

    def test_fission_startup(self):
        if self.check_pref_locked():
            # Need to be able to flip Fission prefs for this test to work.
            return

        # Starting the browser with STARTUP_ENROLLMENT_STATUS set to treatment
        # should make the current session run under treatment.
        with self.full_restart() as profile:
            profile.set_preferences(
                {
                    Prefs.STARTUP_ENROLLMENT_STATUS: ExperimentStatus.ENROLLED_TREATMENT,
                },
                filename="prefs.js",
            )

        self.assertEqual(
            self.marionette.get_pref(Prefs.ENROLLMENT_STATUS),
            ExperimentStatus.UNENROLLED,
            "Dynamic pref should be unenrolled",
        )
        self.assertEqual(
            self.marionette.get_pref(Prefs.STARTUP_ENROLLMENT_STATUS),
            ExperimentStatus.ENROLLED_TREATMENT,
            "Startup pref should be in treatment",
        )
        self.check_fission_status(
            enabled=True,
            experiment=ExperimentStatus.ENROLLED_TREATMENT,
            decision="experimentTreatment",
        )

        # If normandy doesn't re-set `ENROLLMENT_STATUS` during the session, it
        # should be cleared back to disabled after a restart.
        self.marionette.restart(in_app=True, clean=False)

        self.assertEqual(
            self.marionette.get_pref(Prefs.ENROLLMENT_STATUS),
            ExperimentStatus.UNENROLLED,
            "Should unenroll dynamic pref after shutdown",
        )
        self.assertEqual(
            self.marionette.get_pref(Prefs.STARTUP_ENROLLMENT_STATUS),
            ExperimentStatus.UNENROLLED,
            "Should unenroll startup pref after shutdown",
        )
        self.check_fission_status(
            enabled=False,
            experiment=ExperimentStatus.UNENROLLED,
            decision="disabledByDefault",
        )

        # If the browser is started with a customized `fisison.autostart`,
        # while also enrolled in an experiment, the experiment should be
        # disqualified at startup.
        with self.full_restart() as profile:
            profile.set_preferences(
                {
                    Prefs.FISSION_AUTOSTART: True,
                    Prefs.STARTUP_ENROLLMENT_STATUS: ExperimentStatus.ENROLLED_TREATMENT,
                },
                filename="prefs.js",
            )

        self.assertEqual(
            self.marionette.get_pref(Prefs.ENROLLMENT_STATUS),
            ExperimentStatus.DISQUALIFIED,
            "Should disqualify dynamic pref on startup",
        )
        self.assertEqual(
            self.marionette.get_pref(Prefs.STARTUP_ENROLLMENT_STATUS),
            ExperimentStatus.DISQUALIFIED,
            "Should disqualify startup pref on startup",
        )

        self.check_fission_status(
            enabled=True,
            experiment=ExperimentStatus.DISQUALIFIED,
            decision="enabledByUserPref",
        )
