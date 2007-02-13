from buildbot.steps.shell import ShellCommand
from buildbot.status import event, builder

class MaxQ(ShellCommand):
    flunkOnFailure = True
    name = "maxq"

    def __init__(self, testdir=None, **kwargs):
        if not testdir:
            raise TypeError("please pass testdir")
        command = 'run_maxq.py %s' % (testdir,)
        ShellCommand.__init__(self, command=command, **kwargs)

    def startStatus(self):
        evt = event.Event("yellow", ['running', 'maxq', 'tests'],
                      files={'log': self.log})
        self.setCurrentActivity(evt)


    def finished(self, rc):
        self.failures = 0
        if rc:
            self.failures = 1
        output = self.log.getAll()
        self.failures += output.count('\nTEST FAILURE:')

        result = (builder.SUCCESS, ['maxq'])

        if self.failures:
            result = (builder.FAILURE,
                      [str(self.failures), 'maxq', 'failures'])

        return self.stepComplete(result)

    def finishStatus(self, result):
        if self.failures:
            color = "red"
            text = ["maxq", "failed"]
        else:
            color = "green"
            text = ['maxq', 'tests']
        self.updateCurrentActivity(color=color, text=text)
        self.finishStatusSummary()
        self.finishCurrentActivity()


