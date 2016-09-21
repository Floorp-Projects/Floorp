# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/

# The content of this file comes orginally from automationutils.py
# and *should* be revised.

import re
from operator import itemgetter


class ShutdownLeaks(object):

    """
    Parses the mochitest run log when running a debug build, assigns all leaked
    DOM windows (that are still around after test suite shutdown, despite running
    the GC) to the tests that created them and prints leak statistics.
    """

    def __init__(self, logger):
        self.logger = logger
        self.tests = []
        self.leakedWindows = {}
        self.leakedDocShells = set()
        self.currentTest = None
        self.seenShutdown = set()

    def log(self, message):
        if message['action'] == 'log':
            line = message['message']
            if line[2:11] == "DOMWINDOW":
                self._logWindow(line)
            elif line[2:10] == "DOCSHELL":
                self._logDocShell(line)
            elif line.startswith("Completed ShutdownLeaks collections in process"):
                pid = int(line.split()[-1])
                self.seenShutdown.add(pid)
        elif message['action'] == 'test_start':
            fileName = message['test'].replace(
                "chrome://mochitests/content/browser/", "")
            self.currentTest = {
                "fileName": fileName, "windows": set(), "docShells": set()}
        elif message['action'] == 'test_end':
            # don't track a test if no windows or docShells leaked
            if self.currentTest and (self.currentTest["windows"] or self.currentTest["docShells"]):
                self.tests.append(self.currentTest)
            self.currentTest = None

    def process(self):
        if not self.seenShutdown:
            self.logger.warning(
                "TEST-UNEXPECTED-FAIL | ShutdownLeaks | process() called before end of test suite")

        for test in self._parseLeakingTests():
            for url, count in self._zipLeakedWindows(test["leakedWindows"]):
                self.logger.warning(
                    "TEST-UNEXPECTED-FAIL | %s | leaked %d window(s) until shutdown [url = %s]" % (test["fileName"], count, url))

            if test["leakedWindowsString"]:
                self.logger.info("TEST-INFO | %s | windows(s) leaked: %s" %
                                 (test["fileName"], test["leakedWindowsString"]))

            if test["leakedDocShells"]:
                self.logger.warning("TEST-UNEXPECTED-FAIL | %s | leaked %d docShell(s) until shutdown" % (
                    test["fileName"], len(test["leakedDocShells"])))
                self.logger.info("TEST-INFO | %s | docShell(s) leaked: %s" % (test["fileName"],
                                                                              ', '.join(["[pid = %s] [id = %s]" % x for x in test["leakedDocShells"]])))

    def _logWindow(self, line):
        created = line[:2] == "++"
        pid = self._parseValue(line, "pid")
        serial = self._parseValue(line, "serial")

        # log line has invalid format
        if not pid or not serial:
            self.logger.warning(
                "TEST-UNEXPECTED-FAIL | ShutdownLeaks | failed to parse line <%s>" % line)
            return

        key = (pid, serial)

        if self.currentTest:
            windows = self.currentTest["windows"]
            if created:
                windows.add(key)
            else:
                windows.discard(key)
        elif int(pid) in self.seenShutdown and not created:
            self.leakedWindows[key] = self._parseValue(line, "url")

    def _logDocShell(self, line):
        created = line[:2] == "++"
        pid = self._parseValue(line, "pid")
        id = self._parseValue(line, "id")

        # log line has invalid format
        if not pid or not id:
            self.logger.warning(
                "TEST-UNEXPECTED-FAIL | ShutdownLeaks | failed to parse line <%s>" % line)
            return

        key = (pid, id)

        if self.currentTest:
            docShells = self.currentTest["docShells"]
            if created:
                docShells.add(key)
            else:
                docShells.discard(key)
        elif int(pid) in self.seenShutdown and not created:
            self.leakedDocShells.add(key)

    def _parseValue(self, line, name):
        match = re.search("\[%s = (.+?)\]" % name, line)
        if match:
            return match.group(1)
        return None

    def _parseLeakingTests(self):
        leakingTests = []

        for test in self.tests:
            leakedWindows = [
                id for id in test["windows"] if id in self.leakedWindows]
            test["leakedWindows"] = [self.leakedWindows[id]
                                     for id in leakedWindows]
            test["leakedWindowsString"] = ', '.join(
                ["[pid = %s] [serial = %s]" % x for x in leakedWindows])
            test["leakedDocShells"] = [
                id for id in test["docShells"] if id in self.leakedDocShells]
            test["leakCount"] = len(
                test["leakedWindows"]) + len(test["leakedDocShells"])

            if test["leakCount"]:
                leakingTests.append(test)

        return sorted(leakingTests, key=itemgetter("leakCount"), reverse=True)

    def _zipLeakedWindows(self, leakedWindows):
        counts = []
        counted = set()

        for url in leakedWindows:
            if url not in counted:
                counts.append((url, leakedWindows.count(url)))
                counted.add(url)

        return sorted(counts, key=itemgetter(1), reverse=True)


class LSANLeaks(object):

    """
    Parses the log when running an LSAN build, looking for interesting stack frames
    in allocation stacks, and prints out reports.
    """

    def __init__(self, logger):
        self.logger = logger
        self.inReport = False
        self.fatalError = False
        self.foundFrames = set([])
        self.recordMoreFrames = None
        self.currStack = None
        self.maxNumRecordedFrames = 4

        # Don't various allocation-related stack frames, as they do not help much to
        # distinguish different leaks.
        unescapedSkipList = [
            "malloc", "js_malloc", "malloc_", "__interceptor_malloc", "moz_xmalloc",
            "calloc", "js_calloc", "calloc_", "__interceptor_calloc", "moz_xcalloc",
            "realloc", "js_realloc", "realloc_", "__interceptor_realloc", "moz_xrealloc",
            "new",
            "js::MallocProvider",
        ]
        self.skipListRegExp = re.compile(
            "^" + "|".join([re.escape(f) for f in unescapedSkipList]) + "$")

        self.startRegExp = re.compile(
            "==\d+==ERROR: LeakSanitizer: detected memory leaks")
        self.fatalErrorRegExp = re.compile(
            "==\d+==LeakSanitizer has encountered a fatal error.")
        self.stackFrameRegExp = re.compile("    #\d+ 0x[0-9a-f]+ in ([^(</]+)")
        self.sysLibStackFrameRegExp = re.compile(
            "    #\d+ 0x[0-9a-f]+ \(([^+]+)\+0x[0-9a-f]+\)")

    def log(self, line):
        if re.match(self.startRegExp, line):
            self.inReport = True
            return

        if re.match(self.fatalErrorRegExp, line):
            self.fatalError = True
            return

        if not self.inReport:
            return

        if line.startswith("Direct leak") or line.startswith("Indirect leak"):
            self._finishStack()
            self.recordMoreFrames = True
            self.currStack = []
            return

        if line.startswith("SUMMARY: AddressSanitizer"):
            self._finishStack()
            self.inReport = False
            return

        if not self.recordMoreFrames:
            return

        stackFrame = re.match(self.stackFrameRegExp, line)
        if stackFrame:
            # Split the frame to remove any return types.
            frame = stackFrame.group(1).split()[-1]
            if not re.match(self.skipListRegExp, frame):
                self._recordFrame(frame)
            return

        sysLibStackFrame = re.match(self.sysLibStackFrameRegExp, line)
        if sysLibStackFrame:
            # System library stack frames will never match the skip list,
            # so don't bother checking if they do.
            self._recordFrame(sysLibStackFrame.group(1))

        # If we don't match either of these, just ignore the frame.
        # We'll end up with "unknown stack" if everything is ignored.

    def process(self):
        if self.fatalError:
            self.logger.warning(
                "TEST-UNEXPECTED-FAIL | LeakSanitizer | LeakSanitizer has encountered a fatal error.")

        if self.foundFrames:
            self.logger.info("TEST-INFO | LeakSanitizer | To show the addresses of leaked objects add report_objects=1 to LSAN_OPTIONS")
            self.logger.info("TEST-INFO | LeakSanitizer | This can be done in testing/mozbase/mozrunner/mozrunner/utils.py")

        for f in self.foundFrames:
            self.logger.warning(
                "TEST-UNEXPECTED-FAIL | LeakSanitizer | leak at " + f)

    def _finishStack(self):
        if self.recordMoreFrames and len(self.currStack) == 0:
            self.currStack = ["unknown stack"]
        if self.currStack:
            self.foundFrames.add(", ".join(self.currStack))
            self.currStack = None
        self.recordMoreFrames = False
        self.numRecordedFrames = 0

    def _recordFrame(self, frame):
        self.currStack.append(frame)
        self.numRecordedFrames += 1
        if self.numRecordedFrames >= self.maxNumRecordedFrames:
            self.recordMoreFrames = False
