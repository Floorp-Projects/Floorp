from b2g_update_test import B2GUpdateTestCase, OTA, FOTA
import os

update_test_dir = os.path.dirname(os.path.dirname(__file__))

class UpdateTestOTASimple(B2GUpdateTestCase):
    def setUp(self):
        prefs = {
            "b2g.update.apply-idle-timeout": 0
        }
        mar_path = os.path.join(update_test_dir, "unit", "data", "simple.mar")
        self.stage_update(complete_mar=mar_path, prefs=prefs)
        B2GUpdateTestCase.setUp(self)

    def test_ota_simple(self):
        self.marionette.set_script_timeout(60 * 1000 * 5)

        ota_simple_js = os.path.join(os.path.dirname(__file__),
                                     "update_test_ota_simple.js")
        self.execute_update_test(ota_simple_js, apply=OTA)
