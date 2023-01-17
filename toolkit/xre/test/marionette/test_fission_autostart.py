from contextlib import contextmanager

from marionette_harness import MarionetteTestCase


class ExperimentStatus:
    UNENROLLED = 0
    ENROLLED_CONTROL = 1
    ENROLLED_TREATMENT = 2
    DISQUALIFIED = 3


class Prefs:
    FISSION_AUTOSTART = "fission.autostart"
    FISSION_AUTOSTART_SESSION = "fission.autostart.session"


ENV_ENABLE_FISSION = "MOZ_FORCE_ENABLE_FISSION"
ENV_DISABLE_FISSION = "MOZ_FORCE_DISABLE_FISSION"
ENV_DISABLE_E10S = "MOZ_FORCE_DISABLE_E10S"


DECISION_STATUS = {
    "experimentControl": 1,
    "experimentTreatment": 2,
    "disabledByE10sEnv": 3,
    "enabledByEnv": 4,
    "disabledByEnv": 5,
    "enabledByDefault": 7,
    "disabledByDefault": 8,
    "enabledByUserPref": 9,
    "disabledByUserPref": 10,
    "disabledByE10sOther": 11,
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
            decisionStatus: Services.appinfo.fissionDecisionStatus,
            decisionStatusString: Services.appinfo.fissionDecisionStatusString,
            useRemoteSubframes: win.docShell.nsILoadContext.useRemoteSubframes,
            fissionAutostartSession: Services.prefs.getBoolPref("fission.autostart.session"),
            dynamicFissionAutostart: Services.prefs.getBoolPref("fission.autostart"),
          };
        """
        )

    def check_fission_status(self, enabled, decision, dynamic=None):
        if dynamic is None:
            dynamic = enabled

        expected = {
            "fissionAutostart": enabled,
            "decisionStatus": DECISION_STATUS[decision],
            "decisionStatusString": decision,
            "useRemoteSubframes": enabled,
            "fissionAutostartSession": enabled,
            "dynamicFissionAutostart": dynamic,
        }

        status = self.get_fission_status()

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
          window.env = Services.env;
        """
        )

    @contextmanager
    def full_restart(self):
        profile = self.marionette.instance.profile
        try:
            self.marionette.quit()
            yield profile
        finally:
            self.marionette.start_session()
            self.setUpSession()

    def setUp(self):
        super(TestFissionAutostart, self).setUp()

        # If we have configured marionette to require a particular value for
        # `fission.autostart`, remove it as a forced pref until `tearDown`, and
        # perform a clean restart, so we run this test without the pref
        # pre-configured.
        self.fissionRequired = None
        if Prefs.FISSION_AUTOSTART in self.marionette.instance.required_prefs:
            self.fissionRequired = self.marionette.instance.required_prefs[
                Prefs.FISSION_AUTOSTART
            ]
            del self.marionette.instance.required_prefs[Prefs.FISSION_AUTOSTART]
            self.marionette.restart(in_app=False, clean=True)

        self.setUpSession()

        # Fission status must start out with `enabledByDefault`
        self.check_fission_status(
            enabled=True,
            decision="enabledByDefault",
        )

    def tearDown(self):
        if self.fissionRequired is not None:
            self.marionette.instance.required_prefs[
                Prefs.FISSION_AUTOSTART
            ] = self.fissionRequired
        self.marionette.restart(in_app=False, clean=True)

        super(TestFissionAutostart, self).tearDown()

    def test_runtime_changes(self):
        """Tests that changes to preferences during runtime do not have any
        effect on the current session."""

        self.check_fission_status(
            enabled=True,
            decision="enabledByDefault",
        )

        self.restart(prefs={Prefs.FISSION_AUTOSTART: False})

        self.check_fission_status(
            enabled=False,
            decision="disabledByUserPref",
        )

        self.marionette.set_pref(Prefs.FISSION_AUTOSTART, True)
        self.check_fission_status(
            enabled=False,
            decision="disabledByUserPref",
            dynamic=True,
        )

        self.marionette.clear_pref(Prefs.FISSION_AUTOSTART)
        self.check_fission_status(
            enabled=False,
            decision="disabledByUserPref",
            dynamic=True,
        )

        self.restart()
        self.check_fission_status(
            enabled=True,
            decision="enabledByDefault",
        )

    def test_fission_precedence(self):
        self.check_fission_status(
            enabled=True,
            decision="enabledByDefault",
        )

        self.restart(
            prefs={Prefs.FISSION_AUTOSTART: False}, env={ENV_ENABLE_FISSION: "1"}
        )
        self.check_fission_status(
            enabled=True,
            decision="enabledByEnv",
            dynamic=False,
        )

        self.restart(
            prefs={Prefs.FISSION_AUTOSTART: True},
            env={ENV_DISABLE_FISSION: "1", ENV_ENABLE_FISSION: ""},
        )
        self.check_fission_status(
            enabled=False,
            decision="disabledByEnv",
            dynamic=True,
        )

        self.restart(
            prefs={Prefs.FISSION_AUTOSTART: False}, env={ENV_DISABLE_FISSION: ""}
        )
        self.check_fission_status(
            enabled=False,
            decision="disabledByUserPref",
        )

        self.restart(prefs={Prefs.FISSION_AUTOSTART: None})
        self.check_fission_status(
            enabled=True,
            decision="enabledByDefault",
        )

        app_version = self.execute_script("return Services.appinfo.version")
        self.restart(env={ENV_DISABLE_E10S: app_version})
        self.check_fission_status(
            enabled=False,
            decision="disabledByE10sEnv",
            dynamic=True,
        )
