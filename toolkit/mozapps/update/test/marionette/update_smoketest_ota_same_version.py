from b2g_update_test import B2GUpdateSmokeTestCase, OTA, FOTA
import os

this_dir = os.path.dirname(__file__)

class OTASameVersion(B2GUpdateSmokeTestCase):
    JS_PATH          = os.path.join(this_dir, "update_smoketest_ota_same_version.js")
    START_WITH_BUILD = "finish"
    APPLY            = None

    def test_ota_same_version_complete(self):
        self.stage_update(build="finish", mar="complete")
        self.execute_smoketest()

    def test_ota_same_version_partial(self):
        self.stage_update(build="finish", mar="partial")
        self.execute_smoketest()
