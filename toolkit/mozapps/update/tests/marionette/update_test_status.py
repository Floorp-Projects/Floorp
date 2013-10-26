from b2g_update_test import B2GUpdateTestCase, OTA, FOTA
import os

this_dir = os.path.abspath(os.path.dirname(__file__))
update_test_dir = os.path.dirname(this_dir)

class UpdateTestStatus(B2GUpdateTestCase):
    B2G_UPDATES = "/data/local/b2g-updates"

    def setUp(self):
        # Stage a phony update to get the http server up and running
        mar_path = os.path.join(update_test_dir, "unit", "data", "simple.mar")
        self.stage_update(complete_mar=mar_path)

        bad_xml = os.path.join(this_dir, "data", "bad.xml")
        err_cgi = os.path.join(this_dir, "data", "err.cgi")
        self.runner.adb.push(bad_xml, self.B2G_UPDATES + "/bad.xml")
        self.runner.adb.shell("mkdir " + self.B2G_UPDATES + "/cgi-bin")
        self.runner.adb.push(err_cgi, self.B2G_UPDATES + "/cgi-bin/err.cgi")
        self.runner.adb.shell("chmod 755 " + self.B2G_UPDATES + "/cgi-bin/err.cgi")

        B2GUpdateTestCase.setUp(self)

    def test_status(self):
        self.marionette.set_script_timeout(30 * 1000)
        status_js = os.path.join(os.path.dirname(__file__),
                                 "update_test_status.js")
        self.execute_update_test(status_js)

