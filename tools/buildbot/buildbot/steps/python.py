
from buildbot.status.builder import SUCCESS, FAILURE, WARNINGS
from buildbot.steps.shell import ShellCommand

try:
    import cStringIO
    StringIO = cStringIO.StringIO
except ImportError:
    from StringIO import StringIO


class BuildEPYDoc(ShellCommand):
    name = "epydoc"
    command = ["make", "epydocs"]
    description = ["building", "epydocs"]
    descriptionDone = ["epydoc"]

    def createSummary(self, log):
        import_errors = 0
        warnings = 0
        errors = 0

        for line in StringIO(log.getText()):
            if line.startswith("Error importing "):
                import_errors += 1
            if line.find("Warning: ") != -1:
                warnings += 1
            if line.find("Error: ") != -1:
                errors += 1

        self.descriptionDone = self.descriptionDone[:]
        if import_errors:
            self.descriptionDone.append("ierr=%d" % import_errors)
        if warnings:
            self.descriptionDone.append("warn=%d" % warnings)
        if errors:
            self.descriptionDone.append("err=%d" % errors)

        self.import_errors = import_errors
        self.warnings = warnings
        self.errors = errors

    def evaluateCommand(self, cmd):
        if cmd.rc != 0:
            return FAILURE
        if self.warnings or self.errors:
            return WARNINGS
        return SUCCESS


class PyFlakes(ShellCommand):
    name = "pyflakes"
    command = ["make", "pyflakes"]
    description = ["running", "pyflakes"]
    descriptionDone = ["pyflakes"]
    flunkOnFailure = False
    flunkingIssues = ["undefined"] # any pyflakes lines like this cause FAILURE

    MESSAGES = ("unused", "undefined", "redefs", "import*", "misc")

    def createSummary(self, log):
        counts = {}
        summaries = {}
        for m in self.MESSAGES:
            counts[m] = 0
            summaries[m] = []

        first = True
        for line in StringIO(log.getText()).readlines():
            # the first few lines might contain echoed commands from a 'make
            # pyflakes' step, so don't count these as warnings. Stop ignoring
            # the initial lines as soon as we see one with a colon.
            if first:
                if line.find(":") != -1:
                    # there's the colon, this is the first real line
                    first = False
                    # fall through and parse the line
                else:
                    # skip this line, keep skipping non-colon lines
                    continue
            if line.find("imported but unused") != -1:
                m = "unused"
            elif line.find("*' used; unable to detect undefined names") != -1:
                m = "import*"
            elif line.find("undefined name") != -1:
                m = "undefined"
            elif line.find("redefinition of unused") != -1:
                m = "redefs"
            else:
                m = "misc"
            summaries[m].append(line)
            counts[m] += 1

        self.descriptionDone = self.descriptionDone[:]
        for m in self.MESSAGES:
            if counts[m]:
                self.descriptionDone.append("%s=%d" % (m, counts[m]))
                self.addCompleteLog(m, "".join(summaries[m]))
            self.setProperty("pyflakes-%s" % m, counts[m])
        self.setProperty("pyflakes-total", sum(counts.values()))


    def evaluateCommand(self, cmd):
        if cmd.rc != 0:
            return FAILURE
        for m in self.flunkingIssues:
            if self.getProperty("pyflakes-%s" % m):
                return FAILURE
        if self.getProperty("pyflakes-total"):
            return WARNINGS
        return SUCCESS

