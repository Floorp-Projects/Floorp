import unittest
from mozharness.mozilla.release import get_previous_version


class TestGetPreviousVersion(unittest.TestCase):
    def testESR(self):
        self.assertEquals(
            '31.5.3esr',
            get_previous_version('31.6.0esr',
                                 ['31.5.3esr', '31.5.2esr', '31.4.0esr']))

    def testReleaseBuild1(self):
        self.assertEquals(
            '36.0.4',
            get_previous_version('37.0', ['36.0.4', '36.0.1', '35.0.1']))

    def testReleaseBuild2(self):
        self.assertEquals(
            '36.0.4',
            get_previous_version('37.0',
                                 ['37.0', '36.0.4', '36.0.1', '35.0.1']))

    def testBetaMidCycle(self):
        self.assertEquals(
            '37.0b4',
            get_previous_version('37.0b5', ['37.0b4', '37.0b3']))

    def testBetaEarlyCycle(self):
        # 37.0 is the RC build
        self.assertEquals(
            '38.0b1',
            get_previous_version('38.0b2', ['38.0b1', '37.0']))

    def testBetaFirstInCycle(self):
        self.assertEquals(
            '37.0',
            get_previous_version('38.0b1', ['37.0', '37.0b7']))

    def testTwoDots(self):
        self.assertEquals(
            '37.1.0',
            get_previous_version('38.0b1', ['37.1.0', '36.0']))
