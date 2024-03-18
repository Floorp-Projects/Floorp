import logging
import os

logging.getLogger(__name__).addHandler(logging.NullHandler())


class ADBrun(object):
    binary = "adbrun"
    logger = logging.getLogger()

    def launch(self):
        # First close sim if any then launch
        os.system(
            "~/Library/Android/sdk/platform-tools/adb devices | grep emulator | cut -f1 | while read line; do ~/Library/Android/sdk/platform-tools/adb -s $line emu kill; done"
        )
        # Then launch sim
        os.system("sh launchSimScript.sh")
