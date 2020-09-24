from __future__ import absolute_import, print_function

from marionette_harness import MarionetteTestCase


class ExperimentStatus:
    UNKNOWN = 0
    ENROLLED_CONTROL = 1
    ENROLLED_TREATMENT = 2


class Prefs:
    ENROLLMENT_STATUS = 'fission.experiment.enrollmentStatus'
    STARTUP_ENROLLMENT_STATUS = 'fission.experiment.startupEnrollmentStatus'
    FISSION_AUTOSTART = 'fission.autostart'


ENV_ENABLE_FISSION = 'MOZ_FORCE_ENABLE_FISSION'


class TestFissionAutostart(MarionetteTestCase):
    SANDBOX_NAME = 'fission-autostart'

    def execute_script(self, code, *args, **kwargs):
        with self.marionette.using_context(self.marionette.CONTEXT_CHROME):
            return self.marionette.execute_script(code,
                                                  new_sandbox=False,
                                                  sandbox=self.SANDBOX_NAME,
                                                  *args, **kwargs)

    def get_fission_status(self):
        return self.execute_script(r'''
          let win = Services.wm.getMostRecentWindow("navigator:browser");
          return {
            fissionAutostart: Services.appinfo.fissionAutostart,
            fissionExperimentStatus: Services.appinfo.fissionExperimentStatus,
            useRemoteSubframes: win.docShell.nsILoadContext.useRemoteSubframes,
          };
        ''')

    def check_fission_status(self, enabled, experiment):
        expected = {
            'fissionAutostart': enabled,
            'fissionExperimentStatus': experiment,
            'useRemoteSubframes': enabled,
        }

        status = self.get_fission_status()

        for prop, value in expected.items():
            self.assertEqual(
                status[prop], value,
                '%s should have the value `%r`, but has `%r`'
                % (prop, value, status[prop]))

    def set_env(self, env, value):
        self.execute_script('env.set(arguments[0], arguments[1]);',
                            script_args=(env, value))

    def get_env(self, env):
        return self.execute_script('return env.get(arguments[0]);',
                                   script_args=(env,))

    def set_enrollment_status(self, status):
        self.marionette.set_pref(Prefs.ENROLLMENT_STATUS,
                                 status,
                                 default_branch=True)

        startup_status = self.marionette.get_pref(
            Prefs.STARTUP_ENROLLMENT_STATUS)
        self.assertEqual(startup_status, status,
                         'Startup enrollment status (%r) should match new '
                         'session status (%r)' % (startup_status, status))

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
                self.assertEqual(self.marionette.get_pref(key), val)
        if env:
            for key, val in env.items():
                self.assertEqual(self.get_env(key), val or '')

    def setUpSession(self):
        self.marionette.set_context(self.marionette.CONTEXT_CHROME)

        self.execute_script(r'''
          // We're running in a function, in a sandbox, that inherits from an
          // X-ray wrapped window. Anything we want to be globally available
          // needs to be defined on that window.
          ChromeUtils.import("resource://gre/modules/Services.jsm", window);
          window.env = Cc["@mozilla.org/process/environment;1"]
                    .getService(Ci.nsIEnvironment);
        ''')

    def setUp(self):
        super(TestFissionAutostart, self).setUp()

        self.setUpSession()

    def tearDown(self):
        self.marionette.restart(clean=True)

        super(TestFissionAutostart, self).tearDown()

    def test_runtime_changes(self):
        """Tests that changes to preferences during runtime do not have any
        effect on the current session."""

        self.restart(prefs={Prefs.FISSION_AUTOSTART: True})

        self.check_fission_status(enabled=True,
                                  experiment=ExperimentStatus.UNKNOWN)

        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)
        self.check_fission_status(enabled=True,
                                  experiment=ExperimentStatus.UNKNOWN)

        self.marionette.set_pref(Prefs.FISSION_AUTOSTART,
                                 False)
        self.check_fission_status(enabled=True,
                                  experiment=ExperimentStatus.UNKNOWN)

        self.restart()
        self.check_fission_status(enabled=False,
                                  experiment=ExperimentStatus.ENROLLED_CONTROL)

        self.marionette.set_pref(Prefs.ENROLLMENT_STATUS,
                                 ExperimentStatus.UNKNOWN,
                                 default_branch=True)
        self.check_fission_status(enabled=False,
                                  experiment=ExperimentStatus.ENROLLED_CONTROL)

        self.set_env(ENV_ENABLE_FISSION, '1')
        self.check_fission_status(enabled=False,
                                  experiment=ExperimentStatus.ENROLLED_CONTROL)

    def test_fission_precedence(self):
        self.restart(prefs={Prefs.FISSION_AUTOSTART: False},
                     env={ENV_ENABLE_FISSION: '1'})
        self.check_fission_status(enabled=True,
                                  experiment=ExperimentStatus.UNKNOWN)

        self.restart(prefs={Prefs.FISSION_AUTOSTART: True},
                     env={ENV_ENABLE_FISSION: ''})
        self.check_fission_status(enabled=True,
                                  experiment=ExperimentStatus.UNKNOWN)

        self.restart(prefs={Prefs.FISSION_AUTOSTART: False})
        self.check_fission_status(enabled=False,
                                  experiment=ExperimentStatus.UNKNOWN)

        self.set_enrollment_status(ExperimentStatus.ENROLLED_TREATMENT)
        self.restart()
        self.check_fission_status(enabled=True,
                                  experiment=ExperimentStatus.ENROLLED_TREATMENT)

        self.set_enrollment_status(ExperimentStatus.ENROLLED_CONTROL)
        self.restart()
        self.check_fission_status(enabled=False,
                                  experiment=ExperimentStatus.ENROLLED_CONTROL)

        self.restart(prefs={Prefs.FISSION_AUTOSTART: True})
        self.check_fission_status(enabled=False,
                                  experiment=ExperimentStatus.ENROLLED_CONTROL)
