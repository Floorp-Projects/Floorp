import logging
import os
import subprocess

from syncintegration.adbrun import ADBrun

here = os.path.dirname(__file__)
logging.getLogger(__name__).addHandler(logging.NullHandler())


class GradlewBuild(object):
    binary = "./gradlew"
    logger = logging.getLogger()
    adbrun = ADBrun()

    def __init__(self, log):
        self.log = log

    def test(self, identifier, smoke=None):
        # self.adbrun.launch()

        # Change path accordingly to go to root folder to run gradlew
        os.chdir("../../../../../../../..")
        test_type = "ui" if smoke else "experimentintegration"
        cmd = f"adb shell am instrument -w -e class org.mozilla.fenix.{test_type}.{identifier} org.mozilla.fenix.debug.test/androidx.test.runner.AndroidJUnitRunner"
        # if smoke:
        #     cmd = f"adb shell am instrument -w -e class org.mozilla.fenix.ui.{identifier} org.mozilla.fenix.debug.test/androidx.test.runner.AndroidJUnitRunner"    
        # else:
        #     cmd = f"adb shell am instrument -w -e class org.mozilla.fenix.experimentintegration.{identifier} org.mozilla.fenix.debug.test/androidx.test.runner.AndroidJUnitRunner"

        self.logger.info("Running cmd: {}".format(cmd))

        out = ""
        try:
            out = subprocess.check_output(
                cmd, encoding="utf8", shell=True, stderr=subprocess.STDOUT
            )
            if "FAILURES" in out:
                raise (AssertionError(out))
        except subprocess.CalledProcessError as e:
            out = e.output
            raise
        finally:
            # Set the path correctly
            tests_path = (
                "app/src/androidTest/java/org/mozilla/fenix/experimentintegration/"
            )
            os.chdir(tests_path)
            with open(self.log, "w") as f:
                f.write(str(out))
