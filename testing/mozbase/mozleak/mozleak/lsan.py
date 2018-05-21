# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import re


class LSANLeaks(object):

    """
    Parses the log when running an LSAN build, looking for interesting stack frames
    in allocation stacks
    """

    def __init__(self, logger, scope=None, allowed=None):
        self.logger = logger
        self.inReport = False
        self.fatalError = False
        self.symbolizerError = False
        self.foundFrames = set()
        self.recordMoreFrames = None
        self.currStack = None
        self.maxNumRecordedFrames = 4
        self.summaryData = None
        self.scope = scope
        self.allowedMatch = None
        self.sawError = False

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
        self.summaryRegexp = re.compile(
            "SUMMARY: AddressSanitizer: (\d+) byte\(s\) leaked in (\d+) allocation\(s\).")
        self.setAllowed(allowed)

    def setAllowed(self, allowedLines):
        if not allowedLines:
            self.allowedRegexp = None
        else:
            self.allowedRegexp = re.compile(
                "^" + "|".join([re.escape(f) for f in allowedLines]))

    def log(self, line):
        if re.match(self.startRegExp, line):
            self.inReport = True
            # Downgrade this from an ERROR
            self.sawError = True
            return "LeakSanitizer: detected memory leaks"

        if re.match(self.fatalErrorRegExp, line):
            self.fatalError = True
            return line

        if re.match(self.symbolizerOomRegExp, line):
            self.symbolizerError = True
            return line

        if not self.inReport:
            return line

        if line.startswith("Direct leak") or line.startswith("Indirect leak"):
            self._finishStack()
            self.recordMoreFrames = True
            self.currStack = []
            return line

        summaryData = self.summaryRegexp.match(line)
        if summaryData:
            assert self.summaryData is None
            self._finishStack()
            self.inReport = False
            self.summaryData = (int(item) for item in summaryData.groups())
            # We don't return the line here because we want to control whether the
            # leak is seen as an expected failure later
            return

        if not self.recordMoreFrames:
            return line

        stackFrame = re.match(self.stackFrameRegExp, line)
        if stackFrame:
            # Split the frame to remove any return types.
            frame = stackFrame.group(1).split()[-1]
            if not re.match(self.skipListRegExp, frame):
                self._recordFrame(frame)
            return line

        sysLibStackFrame = re.match(self.sysLibStackFrameRegExp, line)
        if sysLibStackFrame:
            # System library stack frames will never match the skip list,
            # so don't bother checking if they do.
            self._recordFrame(sysLibStackFrame.group(1))

        # If we don't match either of these, just ignore the frame.
        # We'll end up with "unknown stack" if everything is ignored.
        return line

    def process(self):
        failures = 0

        if self.summaryData:
            allowed = all(allowed for _, allowed in self.foundFrames)
            self.logger.lsan_summary(*self.summaryData, allowed=allowed)
            self.summaryData = None

        if self.fatalError:
            self.logger.error("LeakSanitizer | LeakSanitizer has encountered a fatal error.")
            failures += 1

        if self.symbolizerError:
            self.logger.error("LeakSanitizer | LLVMSymbolizer was unable to allocate memory.\n"
                              "This will cause leaks that "
                              "should be ignored to instead be reported as an error")
            failures += 1

        if self.foundFrames:
            self.logger.info("LeakSanitizer | To show the "
                             "addresses of leaked objects add report_objects=1 to LSAN_OPTIONS\n"
                             "This can be done in testing/mozbase/mozrunner/mozrunner/utils.py")

            for frames, allowed in self.foundFrames:
                self.logger.lsan_leak(frames, scope=self.scope, allowed_match=allowed)
                if not allowed:
                    failures += 1

        if self.sawError and not (self.summaryData or
                                  self.foundFrames or
                                  self.fatalError or
                                  self.symbolizerError):
            self.logger.error("LeakSanitizer | Memory leaks detected but no leak report generated")

        self.sawError = False

        return failures

    def _finishStack(self):
        if self.recordMoreFrames and len(self.currStack) == 0:
            self.currStack = {"unknown stack"}
        if self.currStack:
            self.foundFrames.add((tuple(self.currStack), self.allowedMatch))
            self.currStack = None
            self.allowedMatch = None
        self.recordMoreFrames = False
        self.numRecordedFrames = 0

    def _recordFrame(self, frame):
        if self.allowedMatch is None and self.allowedRegexp is not None:
            self.allowedMatch = frame if self.allowedRegexp.match(frame) else None
        self.currStack.append(frame)
        self.numRecordedFrames += 1
        if self.numRecordedFrames >= self.maxNumRecordedFrames:
            self.recordMoreFrames = False
