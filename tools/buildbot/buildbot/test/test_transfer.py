# -*- test-case-name: buildbot.test.test_transfer -*-

import os
from stat import ST_MODE
from twisted.trial import unittest
from buildbot.twcompat import maybeWait
from buildbot.steps.transfer import FileUpload, FileDownload
from buildbot.test.runutils import StepTester
from buildbot.status.builder import SUCCESS, FAILURE


# these steps pass a pb.Referenceable inside their arguments, so we have to
# catch and wrap them. If the LocalAsRemote wrapper were a proper membrane,
# we wouldn't have to do this.

class Upload(StepTester, unittest.TestCase):

    def filterArgs(self, args):
        if "writer" in args:
            args["writer"] = self.wrap(args["writer"])
        return args

    def testSuccess(self):
        self.slavebase = "Upload.testSuccess.slave"
        self.masterbase = "Upload.testSuccess.master"
        sb = self.makeSlaveBuilder()
        os.mkdir(os.path.join(self.slavebase, self.slavebuilderbase,
                              "build"))
        # the buildmaster normally runs chdir'ed into masterbase, so uploaded
        # files will appear there. Under trial, we're chdir'ed into
        # _trial_temp instead, so use a different masterdest= to keep the
        # uploaded file in a test-local directory
        masterdest = os.path.join(self.masterbase, "dest.text")
        step = self.makeStep(FileUpload,
                             slavesrc="source.txt",
                             masterdest=masterdest)
        slavesrc = os.path.join(self.slavebase,
                                self.slavebuilderbase,
                                "build",
                                "source.txt")
        contents = "this is the source file\n" * 1000
        open(slavesrc, "w").write(contents)
        f = open(masterdest, "w")
        f.write("overwrite me\n")
        f.close()

        d = self.runStep(step)
        def _checkUpload(results):
            step_status = step.step_status
            #l = step_status.getLogs()
            #if l:
            #    logtext = l[0].getText()
            #    print logtext
            self.failUnlessEqual(results, SUCCESS)
            self.failUnless(os.path.exists(masterdest))
            masterdest_contents = open(masterdest, "r").read()
            self.failUnlessEqual(masterdest_contents, contents)
        d.addCallback(_checkUpload)
        return maybeWait(d)

    def testMaxsize(self):
        self.slavebase = "Upload.testMaxsize.slave"
        self.masterbase = "Upload.testMaxsize.master"
        sb = self.makeSlaveBuilder()
        os.mkdir(os.path.join(self.slavebase, self.slavebuilderbase,
                              "build"))
        masterdest = os.path.join(self.masterbase, "dest2.text")
        step = self.makeStep(FileUpload,
                             slavesrc="source.txt",
                             masterdest=masterdest,
                             maxsize=12345)
        slavesrc = os.path.join(self.slavebase,
                                self.slavebuilderbase,
                                "build",
                                "source.txt")
        contents = "this is the source file\n" * 1000
        open(slavesrc, "w").write(contents)
        f = open(masterdest, "w")
        f.write("overwrite me\n")
        f.close()

        d = self.runStep(step)
        def _checkUpload(results):
            step_status = step.step_status
            #l = step_status.getLogs()
            #if l:
            #    logtext = l[0].getText()
            #    print logtext
            self.failUnlessEqual(results, FAILURE)
            self.failUnless(os.path.exists(masterdest))
            masterdest_contents = open(masterdest, "r").read()
            self.failUnlessEqual(len(masterdest_contents), 12345)
            self.failUnlessEqual(masterdest_contents, contents[:12345])
        d.addCallback(_checkUpload)
        return maybeWait(d)

    def testMode(self):
        self.slavebase = "Upload.testMode.slave"
        self.masterbase = "Upload.testMode.master"
        sb = self.makeSlaveBuilder()
        os.mkdir(os.path.join(self.slavebase, self.slavebuilderbase,
                              "build"))
        masterdest = os.path.join(self.masterbase, "dest3.text")
        step = self.makeStep(FileUpload,
                             slavesrc="source.txt",
                             masterdest=masterdest,
                             mode=0755)
        slavesrc = os.path.join(self.slavebase,
                                self.slavebuilderbase,
                                "build",
                                "source.txt")
        contents = "this is the source file\n"
        open(slavesrc, "w").write(contents)
        f = open(masterdest, "w")
        f.write("overwrite me\n")
        f.close()

        d = self.runStep(step)
        def _checkUpload(results):
            step_status = step.step_status
            #l = step_status.getLogs()
            #if l:
            #    logtext = l[0].getText()
            #    print logtext
            self.failUnlessEqual(results, SUCCESS)
            self.failUnless(os.path.exists(masterdest))
            masterdest_contents = open(masterdest, "r").read()
            self.failUnlessEqual(masterdest_contents, contents)
            # and with 0777 to ignore sticky bits
            dest_mode = os.stat(masterdest)[ST_MODE] & 0777
            self.failUnlessEqual(dest_mode, 0755,
                                 "target mode was %o, we wanted %o" %
                                 (dest_mode, 0755))
        d.addCallback(_checkUpload)
        return maybeWait(d)

    def testMissingFile(self):
        self.slavebase = "Upload.testMissingFile.slave"
        self.masterbase = "Upload.testMissingFile.master"
        sb = self.makeSlaveBuilder()
        step = self.makeStep(FileUpload,
                             slavesrc="MISSING.txt",
                             masterdest="dest.txt")
        masterdest = os.path.join(self.masterbase, "dest4.txt")

        d = self.runStep(step)
        def _checkUpload(results):
            step_status = step.step_status
            self.failUnlessEqual(results, FAILURE)
            self.failIf(os.path.exists(masterdest))
            l = step_status.getLogs()
            logtext = l[0].getText().strip()
            self.failUnless(logtext.startswith("Cannot open file"))
            self.failUnless(logtext.endswith("for upload"))
        d.addCallback(_checkUpload)
        return maybeWait(d)

    

class Download(StepTester, unittest.TestCase):

    def filterArgs(self, args):
        if "reader" in args:
            args["reader"] = self.wrap(args["reader"])
        return args

    def testSuccess(self):
        self.slavebase = "Download.testSuccess.slave"
        self.masterbase = "Download.testSuccess.master"
        sb = self.makeSlaveBuilder()
        os.mkdir(os.path.join(self.slavebase, self.slavebuilderbase,
                              "build"))
        mastersrc = os.path.join(self.masterbase, "source.text")
        slavedest = os.path.join(self.slavebase,
                                 self.slavebuilderbase,
                                 "build",
                                 "dest.txt")
        step = self.makeStep(FileDownload,
                             mastersrc=mastersrc,
                             slavedest="dest.txt")
        contents = "this is the source file\n" * 1000  # 24kb, so two blocks
        open(mastersrc, "w").write(contents)
        f = open(slavedest, "w")
        f.write("overwrite me\n")
        f.close()

        d = self.runStep(step)
        def _checkDownload(results):
            step_status = step.step_status
            self.failUnlessEqual(results, SUCCESS)
            self.failUnless(os.path.exists(slavedest))
            slavedest_contents = open(slavedest, "r").read()
            self.failUnlessEqual(slavedest_contents, contents)
        d.addCallback(_checkDownload)
        return maybeWait(d)

    def testMaxsize(self):
        self.slavebase = "Download.testMaxsize.slave"
        self.masterbase = "Download.testMaxsize.master"
        sb = self.makeSlaveBuilder()
        os.mkdir(os.path.join(self.slavebase, self.slavebuilderbase,
                              "build"))
        mastersrc = os.path.join(self.masterbase, "source.text")
        slavedest = os.path.join(self.slavebase,
                                 self.slavebuilderbase,
                                 "build",
                                 "dest.txt")
        step = self.makeStep(FileDownload,
                             mastersrc=mastersrc,
                             slavedest="dest.txt",
                             maxsize=12345)
        contents = "this is the source file\n" * 1000  # 24kb, so two blocks
        open(mastersrc, "w").write(contents)
        f = open(slavedest, "w")
        f.write("overwrite me\n")
        f.close()

        d = self.runStep(step)
        def _checkDownload(results):
            step_status = step.step_status
            # the file should be truncated, and the step a FAILURE
            self.failUnlessEqual(results, FAILURE)
            self.failUnless(os.path.exists(slavedest))
            slavedest_contents = open(slavedest, "r").read()
            self.failUnlessEqual(len(slavedest_contents), 12345)
            self.failUnlessEqual(slavedest_contents, contents[:12345])
        d.addCallback(_checkDownload)
        return maybeWait(d)

    def testMode(self):
        self.slavebase = "Download.testMode.slave"
        self.masterbase = "Download.testMode.master"
        sb = self.makeSlaveBuilder()
        os.mkdir(os.path.join(self.slavebase, self.slavebuilderbase,
                              "build"))
        mastersrc = os.path.join(self.masterbase, "source.text")
        slavedest = os.path.join(self.slavebase,
                                 self.slavebuilderbase,
                                 "build",
                                 "dest.txt")
        step = self.makeStep(FileDownload,
                             mastersrc=mastersrc,
                             slavedest="dest.txt",
                             mode=0755)
        contents = "this is the source file\n"
        open(mastersrc, "w").write(contents)
        f = open(slavedest, "w")
        f.write("overwrite me\n")
        f.close()

        d = self.runStep(step)
        def _checkDownload(results):
            step_status = step.step_status
            self.failUnlessEqual(results, SUCCESS)
            self.failUnless(os.path.exists(slavedest))
            slavedest_contents = open(slavedest, "r").read()
            self.failUnlessEqual(slavedest_contents, contents)
            # and with 0777 to ignore sticky bits
            dest_mode = os.stat(slavedest)[ST_MODE] & 0777
            self.failUnlessEqual(dest_mode, 0755,
                                 "target mode was %o, we wanted %o" %
                                 (dest_mode, 0755))
        d.addCallback(_checkDownload)
        return maybeWait(d)

    def testMissingFile(self):
        self.slavebase = "Download.testMissingFile.slave"
        self.masterbase = "Download.testMissingFile.master"
        sb = self.makeSlaveBuilder()
        os.mkdir(os.path.join(self.slavebase, self.slavebuilderbase,
                              "build"))
        mastersrc = os.path.join(self.masterbase, "MISSING.text")
        slavedest = os.path.join(self.slavebase,
                                 self.slavebuilderbase,
                                 "build",
                                 "dest.txt")
        step = self.makeStep(FileDownload,
                             mastersrc=mastersrc,
                             slavedest="dest.txt")

        d = self.runStep(step)
        def _checkDownload(results):
            step_status = step.step_status
            self.failUnlessEqual(results, FAILURE)
            self.failIf(os.path.exists(slavedest))
            l = step_status.getLogs()
            logtext = l[0].getText().strip()
            self.failUnless(logtext.endswith(" not available at master"))
        d.addCallbacks(_checkDownload)

        return maybeWait(d)


# TODO:
#  test relative paths, ~/paths
#   need to implement expanduser() for slave-side
#  test error message when master-side file is in a missing directory
#  remove workdir= default?

