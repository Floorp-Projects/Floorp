import gc
import unittest


import mozharness.base.log as log
from mozharness.base.log import ERROR
import mozharness.base.script as script
from mozharness.mozilla.automation import AutomationMixin


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


class AutomationScript(AutomationMixin, script.BaseScript):
    def __init__(self, **kwargs):
        super(AutomationScript, self).__init__(**kwargs)


# TestAutomationStatus {{{1
class TestAutomationStatus(unittest.TestCase):
    # I need a log watcher helper function, here and in test_log.
    def setUp(self):
        cleanup()
        self.s = None

    def tearDown(self):
        # Close the logfile handles, or windows can't remove the logs
        if hasattr(self, 's') and isinstance(self.s, object):
            del(self.s)
        cleanup()


# main {{{1
if __name__ == '__main__':
    unittest.main()
