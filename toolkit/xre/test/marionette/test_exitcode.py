from marionette_harness import MarionetteTestCase


class TestFissionAutostart(MarionetteTestCase):
    def test_normal_exit(self):
        self.marionette.set_context(self.marionette.CONTEXT_CHROME)

        def call_quit():
            self.marionette.execute_script(
                """
                Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit);
                """,
                sandbox="system",
            )

        self.marionette.quit(in_app=True, callback=call_quit)
        self.assertEqual(self.marionette.instance.runner.returncode, 0)

    def test_exit_code(self):
        self.marionette.set_context(self.marionette.CONTEXT_CHROME)

        def call_quit():
            self.marionette.execute_script(
                """
                Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit, 5);
                """,
                sandbox="system",
            )

        self.marionette.quit(in_app=True, callback=call_quit)
        self.assertEqual(self.marionette.instance.runner.returncode, 5)
