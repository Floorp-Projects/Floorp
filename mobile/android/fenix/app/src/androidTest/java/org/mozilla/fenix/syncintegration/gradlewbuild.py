import logging
import os
import subprocess

from adbrun import ADBrun

here = os.path.dirname(__file__)
logging.getLogger(__name__).addHandler(logging.NullHandler())


class GradlewBuild(object):
    binary = './gradlew'
    logger = logging.getLogger()
    adbrun = ADBrun()

    def __init__(self, log):
        self.log = log

    def test(self, identifier):
        self.adbrun.launch()
        # Change path accordingly to go to root folder to run gradlew
        os.chdir('../../../../../../../..')
        args = './gradlew ' + 'app:connectedGeckoNightlyDebugAndroidTest -Pandroid.testInstrumentationRunnerArguments.class=org.mozilla.fenix.syncintegration.SyncIntegrationTest#{}'.format(identifier)

        self.logger.info('Running: {}'.format(' '.join(args)))
        try:
            out = subprocess.check_output(
                args, shell=True)
        except subprocess.CalledProcessError as e:
            out = e.output
            raise
        finally:
            with open(self.log, 'w') as f:
                f.writelines(out)

