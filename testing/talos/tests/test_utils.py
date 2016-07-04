from talos import utils
import unittest
import os


class TestTimer(unittest.TestCase):
    def test_timer(self):
        timer = utils.Timer()
        timer._start_time -= 3  # remove three seconds for the test
        self.assertEquals(timer.elapsed(), '00:00:03')


class TestRestoreEnv(unittest.TestCase):
    def test_basic(self):
        env_var = 'THIS_IS_A_ENV_VAR_NOT_USED'
        self.assertNotIn(env_var, os.environ)
        with utils.restore_environment_vars():
            os.environ[env_var] = '1'
        self.assertNotIn(env_var, os.environ)


class TestInterpolate(unittest.TestCase):
    def test_interpolate_talos_is_always_defines(self):
        self.assertEquals(utils.interpolate('${talos}'), utils.here)

    def test_interpolate_custom_placeholders(self):
        self.assertEquals(utils.interpolate('${talos} ${foo} abc', foo='bar', unused=1),
                          utils.here + ' bar abc')


class TestParsePref(unittest.TestCase):
    def test_parse_string(self):
        self.assertEquals(utils.parse_pref('abc'), 'abc')

    def test_parse_int(self):
        self.assertEquals(utils.parse_pref('12'), 12)

    def test_parse_bool(self):
        self.assertEquals(utils.parse_pref('true'), True)
        self.assertEquals(utils.parse_pref('false'), False)
