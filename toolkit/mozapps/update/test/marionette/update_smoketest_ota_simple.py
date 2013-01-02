from b2g_update_test import B2GUpdateSmokeTestCase, OTA, FOTA
import os

this_dir = os.path.dirname(__file__)

class OTASimple(B2GUpdateSmokeTestCase):
    JS_PATH          = os.path.join(this_dir, "update_smoketest_ota_simple.js")
    START_WITH_BUILD = "start"
    STAGE_PREFS      = {
        "b2g.update.apply-idle-timeout": 0,
        "b2g.update.apply-prompt-timeout": 5 * 60 * 1000
    }
    TIMEOUT     = 5 * 60 * 1000

    def test_ota_simple_complete(self):
        self.stage_update(build="finish", mar="complete")
        self.execute_smoketest()

    def test_ota_simple_partial(self):
        self.stage_update(build="finish", mar="partial")
        self.execute_smoketest()
