import gc
import unittest


import mozharness.base.log as log
from mozharness.base.log import ERROR
import mozharness.base.script as script
from mozharness.mozilla.buildbot import BuildbotMixin, TBPL_SUCCESS, \
    TBPL_FAILURE, EXIT_STATUS_DICT


class CleanupObj(script.ScriptMixin, log.LogMixin):
    def __init__(self):
        super(CleanupObj, self).__init__()
        self.log_obj = None
        self.config = {'log_level': ERROR}


def cleanup():
    gc.collect()
    c = CleanupObj()
    for f in ('test_logs', 'test_dir', 'tmpfile_stdout', 'tmpfile_stderr'):
        c.rmtree(f)


class BuildbotScript(BuildbotMixin, script.BaseScript):
    def __init__(self, **kwargs):
        super(BuildbotScript, self).__init__(**kwargs)


# TestBuildbotStatus {{{1
class TestBuildbotStatus(unittest.TestCase):
    # I need a log watcher helper function, here and in test_log.
    def setUp(self):
        cleanup()
        self.s = None

    def tearDown(self):
        # Close the logfile handles, or windows can't remove the logs
        if hasattr(self, 's') and isinstance(self.s, object):
            del(self.s)
        cleanup()

    def test_over_max_log_size(self):
        self.s = BuildbotScript(config={'log_type': 'multi',
                                        'buildbot_max_log_size': 200},
                                initial_config_file='test/test.json')
        self.s.info("foo!")
        self.s.buildbot_status(TBPL_SUCCESS)
        self.assertEqual(self.s.return_code, EXIT_STATUS_DICT[TBPL_FAILURE])

    def test_under_max_log_size(self):
        self.s = BuildbotScript(config={'log_type': 'multi',
                                        'buildbot_max_log_size': 20000},
                                initial_config_file='test/test.json')
        self.s.info("foo!")
        self.s.buildbot_status(TBPL_SUCCESS)
        self.assertEqual(self.s.return_code, EXIT_STATUS_DICT[TBPL_SUCCESS])

# main {{{1
if __name__ == '__main__':
    unittest.main()
