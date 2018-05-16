import re


class LSANLeaks(object):

    """
    Parses the log when running an LSAN build, looking for interesting stack frames
    in allocation stacks, and prints out reports.
    """

    def __init__(self, logger):
        self.logger = logger
        self.inReport = False
        self.fatalError = False
        self.symbolizerError = False
        self.foundFrames = set([])
        self.recordMoreFrames = None
        self.currStack = None
        self.allowedMatch = None
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
        self.symbolizerOomRegExp = re.compile(
            "LLVMSymbolizer: error reading file: Cannot allocate memory")
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

        if re.match(self.symbolizerOomRegExp, line):
            self.symbolizerError = True
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
        failures = 0

        if self.fatalError:
            self.logger.error("TEST-UNEXPECTED-FAIL | LeakSanitizer | LeakSanitizer "
                              "has encountered a fatal error.")
            failures += 1

        if self.symbolizerError:
            self.logger.error("TEST-UNEXPECTED-FAIL | LeakSanitizer | LLVMSymbolizer "
                              "was unable to allocate memory.")
            failures += 1
            self.logger.info("TEST-INFO | LeakSanitizer | This will cause leaks that "
                             "should be ignored to instead be reported as an error")

        if self.foundFrames:
            self.logger.info("TEST-INFO | LeakSanitizer | To show the "
                             "addresses of leaked objects add report_objects=1 to LSAN_OPTIONS")
            self.logger.info("TEST-INFO | LeakSanitizer | This can be done "
                             "in testing/mozbase/mozrunner/mozrunner/utils.py")

        for f in self.foundFrames:
            self.logger.error(
                "TEST-UNEXPECTED-FAIL | LeakSanitizer | leak at " + f)
            failures += 1

        return failures

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
