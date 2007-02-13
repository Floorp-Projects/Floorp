# -*- test-case-name: buildbot.test.test_mailparse -*-

from twisted.trial import unittest
from twisted.python import util
from buildbot.changes.mail import parseFreshCVSMail, parseSyncmail

class Test1(unittest.TestCase):

    def get(self, msg):
        msg = util.sibpath(__file__, msg)
        return parseFreshCVSMail(None, open(msg, "r"))

    def testMsg1(self):
        c = self.get("mail/msg1")
        self.assertEqual(c.who, "moshez")
        self.assertEqual(c.files, ["Twisted/debian/python-twisted.menu.in"])
        self.assertEqual(c.comments, "Instance massenger, apparently\n")
        self.assertEqual(c.isdir, 0)

    def testMsg2(self):
        c = self.get("mail/msg2")
        self.assertEqual(c.who, "itamarst")
        self.assertEqual(c.files, ["Twisted/twisted/web/woven/form.py",
                                   "Twisted/twisted/python/formmethod.py"])
        self.assertEqual(c.comments,
                         "submit formmethod now subclass of Choice\n")
        self.assertEqual(c.isdir, 0)

    def testMsg3(self):
        # same as msg2 but missing the ViewCVS section
        c = self.get("mail/msg3")
        self.assertEqual(c.who, "itamarst")
        self.assertEqual(c.files, ["Twisted/twisted/web/woven/form.py",
                                   "Twisted/twisted/python/formmethod.py"])
        self.assertEqual(c.comments,
                         "submit formmethod now subclass of Choice\n")
        self.assertEqual(c.isdir, 0)

    def testMsg4(self):
        # same as msg3 but also missing CVS patch section
        c = self.get("mail/msg4")
        self.assertEqual(c.who, "itamarst")
        self.assertEqual(c.files, ["Twisted/twisted/web/woven/form.py",
                                   "Twisted/twisted/python/formmethod.py"])
        self.assertEqual(c.comments,
                         "submit formmethod now subclass of Choice\n")
        self.assertEqual(c.isdir, 0)

    def testMsg5(self):
        # creates a directory
        c = self.get("mail/msg5")
        self.assertEqual(c.who, "etrepum")
        self.assertEqual(c.files, ["Twisted/doc/examples/cocoaDemo"])
        self.assertEqual(c.comments,
                         "Directory /cvs/Twisted/doc/examples/cocoaDemo added to the repository\n")
        self.assertEqual(c.isdir, 1)

    def testMsg6(self):
        # adds files
        c = self.get("mail/msg6")
        self.assertEqual(c.who, "etrepum")
        self.assertEqual(c.files, [
            "Twisted/doc/examples/cocoaDemo/MyAppDelegate.py",
            "Twisted/doc/examples/cocoaDemo/__main__.py",
            "Twisted/doc/examples/cocoaDemo/bin-python-main.m",
            "Twisted/doc/examples/cocoaDemo/English.lproj/InfoPlist.strings",
            "Twisted/doc/examples/cocoaDemo/English.lproj/MainMenu.nib/classes.nib",
            "Twisted/doc/examples/cocoaDemo/English.lproj/MainMenu.nib/info.nib",
            "Twisted/doc/examples/cocoaDemo/English.lproj/MainMenu.nib/keyedobjects.nib",
            "Twisted/doc/examples/cocoaDemo/cocoaDemo.pbproj/project.pbxproj"])
        self.assertEqual(c.comments,
                         "Cocoa (OS X) clone of the QT demo, using polling reactor\n\nRequires pyobjc ( http://pyobjc.sourceforge.net ), it's not much different than the template project.  The reactor is iterated periodically by a repeating NSTimer.\n")
        self.assertEqual(c.isdir, 0)

    def testMsg7(self):
        # deletes files
        c = self.get("mail/msg7")
        self.assertEqual(c.who, "etrepum")
        self.assertEqual(c.files, [
            "Twisted/doc/examples/cocoaDemo/MyAppDelegate.py",
            "Twisted/doc/examples/cocoaDemo/__main__.py",
            "Twisted/doc/examples/cocoaDemo/bin-python-main.m",
            "Twisted/doc/examples/cocoaDemo/English.lproj/InfoPlist.strings",
            "Twisted/doc/examples/cocoaDemo/English.lproj/MainMenu.nib/classes.nib",
            "Twisted/doc/examples/cocoaDemo/English.lproj/MainMenu.nib/info.nib",
            "Twisted/doc/examples/cocoaDemo/English.lproj/MainMenu.nib/keyedobjects.nib",
            "Twisted/doc/examples/cocoaDemo/cocoaDemo.pbproj/project.pbxproj"])
        self.assertEqual(c.comments,
                         "Directories break debian build script, waiting for reasonable fix\n")
        self.assertEqual(c.isdir, 0)

    def testMsg8(self):
        # files outside Twisted/
        c = self.get("mail/msg8")
        self.assertEqual(c.who, "acapnotic")
        self.assertEqual(c.files, [ "CVSROOT/freshCfg" ])
        self.assertEqual(c.comments, "it doesn't work with invalid syntax\n")
        self.assertEqual(c.isdir, 0)

    def testMsg9(self):
        # also creates a directory
        c = self.get("mail/msg9")
        self.assertEqual(c.who, "exarkun")
        self.assertEqual(c.files, ["Twisted/sandbox/exarkun/persist-plugin"])
        self.assertEqual(c.comments,
                         "Directory /cvs/Twisted/sandbox/exarkun/persist-plugin added to the repository\n")
        self.assertEqual(c.isdir, 1)


class Test2(unittest.TestCase):
    def get(self, msg):
        msg = util.sibpath(__file__, msg)
        return parseFreshCVSMail(None, open(msg, "r"), prefix="Twisted")

    def testMsg1p(self):
        c = self.get("mail/msg1")
        self.assertEqual(c.who, "moshez")
        self.assertEqual(c.files, ["debian/python-twisted.menu.in"])
        self.assertEqual(c.comments, "Instance massenger, apparently\n")

    def testMsg2p(self):
        c = self.get("mail/msg2")
        self.assertEqual(c.who, "itamarst")
        self.assertEqual(c.files, ["twisted/web/woven/form.py",
                                   "twisted/python/formmethod.py"])
        self.assertEqual(c.comments,
                         "submit formmethod now subclass of Choice\n")

    def testMsg3p(self):
        # same as msg2 but missing the ViewCVS section
        c = self.get("mail/msg3")
        self.assertEqual(c.who, "itamarst")
        self.assertEqual(c.files, ["twisted/web/woven/form.py",
                                   "twisted/python/formmethod.py"])
        self.assertEqual(c.comments,
                         "submit formmethod now subclass of Choice\n")

    def testMsg4p(self):
        # same as msg3 but also missing CVS patch section
        c = self.get("mail/msg4")
        self.assertEqual(c.who, "itamarst")
        self.assertEqual(c.files, ["twisted/web/woven/form.py",
                                   "twisted/python/formmethod.py"])
        self.assertEqual(c.comments,
                         "submit formmethod now subclass of Choice\n")

    def testMsg5p(self):
        # creates a directory
        c = self.get("mail/msg5")
        self.assertEqual(c.who, "etrepum")
        self.assertEqual(c.files, ["doc/examples/cocoaDemo"])
        self.assertEqual(c.comments,
                         "Directory /cvs/Twisted/doc/examples/cocoaDemo added to the repository\n")
        self.assertEqual(c.isdir, 1)

    def testMsg6p(self):
        # adds files
        c = self.get("mail/msg6")
        self.assertEqual(c.who, "etrepum")
        self.assertEqual(c.files, [
            "doc/examples/cocoaDemo/MyAppDelegate.py",
            "doc/examples/cocoaDemo/__main__.py",
            "doc/examples/cocoaDemo/bin-python-main.m",
            "doc/examples/cocoaDemo/English.lproj/InfoPlist.strings",
            "doc/examples/cocoaDemo/English.lproj/MainMenu.nib/classes.nib",
            "doc/examples/cocoaDemo/English.lproj/MainMenu.nib/info.nib",
            "doc/examples/cocoaDemo/English.lproj/MainMenu.nib/keyedobjects.nib",
            "doc/examples/cocoaDemo/cocoaDemo.pbproj/project.pbxproj"])
        self.assertEqual(c.comments,
                         "Cocoa (OS X) clone of the QT demo, using polling reactor\n\nRequires pyobjc ( http://pyobjc.sourceforge.net ), it's not much different than the template project.  The reactor is iterated periodically by a repeating NSTimer.\n")
        self.assertEqual(c.isdir, 0)

    def testMsg7p(self):
        # deletes files
        c = self.get("mail/msg7")
        self.assertEqual(c.who, "etrepum")
        self.assertEqual(c.files, [
            "doc/examples/cocoaDemo/MyAppDelegate.py",
            "doc/examples/cocoaDemo/__main__.py",
            "doc/examples/cocoaDemo/bin-python-main.m",
            "doc/examples/cocoaDemo/English.lproj/InfoPlist.strings",
            "doc/examples/cocoaDemo/English.lproj/MainMenu.nib/classes.nib",
            "doc/examples/cocoaDemo/English.lproj/MainMenu.nib/info.nib",
            "doc/examples/cocoaDemo/English.lproj/MainMenu.nib/keyedobjects.nib",
            "doc/examples/cocoaDemo/cocoaDemo.pbproj/project.pbxproj"])
        self.assertEqual(c.comments,
                         "Directories break debian build script, waiting for reasonable fix\n")
        self.assertEqual(c.isdir, 0)

    def testMsg8p(self):
        # files outside Twisted/
        c = self.get("mail/msg8")
        self.assertEqual(c, None)


class Test3(unittest.TestCase):
    def get(self, msg):
        msg = util.sibpath(__file__, msg)
        return parseSyncmail(None, open(msg, "r"), prefix="buildbot")

    def getNoPrefix(self, msg):
        msg = util.sibpath(__file__, msg)
        return parseSyncmail(None, open(msg, "r"))

    def testMsgS1(self):
        c = self.get("mail/syncmail.1")
        self.failUnless(c is not None)
        self.assertEqual(c.who, "warner")
        self.assertEqual(c.files, ["buildbot/changes/freshcvsmail.py"])
        self.assertEqual(c.comments,
                         "remove leftover code, leave a temporary compatibility import. Note! Start\nimporting FCMaildirSource from changes.mail instead of changes.freshcvsmail\n")
        self.assertEqual(c.isdir, 0)

    def testMsgS2(self):
        c = self.get("mail/syncmail.2")
        self.assertEqual(c.who, "warner")
        self.assertEqual(c.files, ["ChangeLog"])
        self.assertEqual(c.comments, "\t* NEWS: started adding new features\n")
        self.assertEqual(c.isdir, 0)

    def testMsgS3(self):
        c = self.get("mail/syncmail.3")
        self.failUnless(c == None)

    def testMsgS4(self):
        c = self.get("mail/syncmail.4")
        self.assertEqual(c.who, "warner")
        self.assertEqual(c.files, ["test/mail/syncmail.1",
                                   "test/mail/syncmail.2",
                                   "test/mail/syncmail.3"
                                   ])
        self.assertEqual(c.comments, "test cases for syncmail parser\n")
        self.assertEqual(c.isdir, 0)
        self.assertEqual(c.branch, None)

    # tests a tag
    def testMsgS5(self):
        c = self.getNoPrefix("mail/syncmail.5")
        self.failUnless(c)
        self.assertEqual(c.who, "thomas")
        self.assertEqual(c.files, ['test1/MANIFEST',
                                   'test1/Makefile.am',
                                   'test1/autogen.sh',
                                   'test1/configure.in' 
                                   ])
        self.assertEqual(c.branch, "BRANCH-DEVEL")
        self.assertEqual(c.isdir, 0)
