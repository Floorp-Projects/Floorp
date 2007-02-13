
# this file tests the 'buildbot' command, with its various sub-commands

from twisted.trial import unittest
from twisted.python import usage
import os, shutil, shlex

from buildbot.scripts import runner, tryclient

class Options(unittest.TestCase):
    optionsFile = "SDFsfsFSdfsfsFSD"

    def make(self, d, key):
        # we use a wacky filename here in case the test code discovers the
        # user's real ~/.buildbot/ directory
        os.makedirs(os.sep.join(d + [".buildbot"]))
        f = open(os.sep.join(d + [".buildbot", self.optionsFile]), "w")
        f.write("key = '%s'\n" % key)
        f.close()

    def check(self, d, key):
        basedir = os.sep.join(d)
        options = runner.loadOptions(self.optionsFile, here=basedir,
                                     home=self.home)
        if key is None:
            self.failIf(options.has_key('key'))
        else:
            self.failUnlessEqual(options['key'], key)

    def testFindOptions(self):
        self.make(["home", "dir1", "dir2", "dir3"], "one")
        self.make(["home", "dir1", "dir2"], "two")
        self.make(["home"], "home")
        self.home = os.path.abspath("home")

        self.check(["home", "dir1", "dir2", "dir3"], "one")
        self.check(["home", "dir1", "dir2"], "two")
        self.check(["home", "dir1"], "home")

        self.home = os.path.abspath("nothome")
        os.makedirs(os.sep.join(["nothome", "dir1"]))
        self.check(["nothome", "dir1"], None)

    def doForce(self, args, expected):
        o = runner.ForceOptions()
        o.parseOptions(args)
        self.failUnlessEqual(o.keys(), expected.keys())
        for k in o.keys():
            self.failUnlessEqual(o[k], expected[k],
                                 "[%s] got %s instead of %s" % (k, o[k],
                                                                expected[k]))

    def testForceOptions(self):
        if not hasattr(shlex, "split"):
            raise unittest.SkipTest("need python>=2.3 for shlex.split")

        exp = {"builder": "b1", "reason": "reason",
               "branch": None, "revision": None}
        self.doForce(shlex.split("b1 reason"), exp)
        self.doForce(shlex.split("b1 'reason'"), exp)
        self.failUnlessRaises(usage.UsageError, self.doForce,
                              shlex.split("--builder b1 'reason'"), exp)
        self.doForce(shlex.split("--builder b1 --reason reason"), exp)
        self.doForce(shlex.split("--builder b1 --reason 'reason'"), exp)
        self.doForce(shlex.split("--builder b1 --reason \"reason\""), exp)
        
        exp['reason'] = "longer reason"
        self.doForce(shlex.split("b1 'longer reason'"), exp)
        self.doForce(shlex.split("b1 longer reason"), exp)
        self.doForce(shlex.split("--reason 'longer reason' b1"), exp)
        

class Create(unittest.TestCase):
    def failUnlessIn(self, substring, string, msg=None):
        # trial provides a version of this that requires python-2.3 to test
        # strings.
        self.failUnless(string.find(substring) != -1, msg)
    def failUnlessExists(self, filename):
        self.failUnless(os.path.exists(filename), "%s should exist" % filename)
    def failIfExists(self, filename):
        self.failIf(os.path.exists(filename), "%s should not exist" % filename)

    def testMaster(self):
        basedir = "test_runner.master"
        options = runner.MasterOptions()
        options.parseOptions(["-q", basedir])
        cwd = os.getcwd()
        runner.createMaster(options)
        os.chdir(cwd)

        tac = os.path.join(basedir, "buildbot.tac")
        self.failUnless(os.path.exists(tac))
        tacfile = open(tac,"rt").read()
        self.failUnlessIn("basedir", tacfile)
        self.failUnlessIn("configfile = r'master.cfg'", tacfile)
        self.failUnlessIn("BuildMaster(basedir, configfile)", tacfile)

        cfg = os.path.join(basedir, "master.cfg")
        self.failIfExists(cfg)
        samplecfg = os.path.join(basedir, "master.cfg.sample")
        self.failUnlessExists(samplecfg)
        cfgfile = open(samplecfg,"rt").read()
        self.failUnlessIn("This is a sample buildmaster config file", cfgfile)

        makefile = os.path.join(basedir, "Makefile.sample")
        self.failUnlessExists(makefile)

        # now verify that running it a second time (with the same options)
        # does the right thing: nothing changes
        runner.createMaster(options)
        os.chdir(cwd)

        self.failIfExists(os.path.join(basedir, "buildbot.tac.new"))
        self.failUnlessExists(os.path.join(basedir, "master.cfg.sample"))

        oldtac = open(os.path.join(basedir, "buildbot.tac"), "rt").read()

        # mutate Makefile.sample, since it should be rewritten
        f = open(os.path.join(basedir, "Makefile.sample"), "rt")
        oldmake = f.read()
        f = open(os.path.join(basedir, "Makefile.sample"), "wt")
        f.write(oldmake)
        f.write("# additional line added\n")
        f.close()

        # also mutate master.cfg.sample
        f = open(os.path.join(basedir, "master.cfg.sample"), "rt")
        oldsamplecfg = f.read()
        f = open(os.path.join(basedir, "master.cfg.sample"), "wt")
        f.write(oldsamplecfg)
        f.write("# additional line added\n")
        f.close()

        # now run it again (with different options)
        options = runner.MasterOptions()
        options.parseOptions(["-q", "--config", "other.cfg", basedir])
        runner.createMaster(options)
        os.chdir(cwd)

        tac = open(os.path.join(basedir, "buildbot.tac"), "rt").read()
        self.failUnlessEqual(tac, oldtac, "shouldn't change existing .tac")
        self.failUnlessExists(os.path.join(basedir, "buildbot.tac.new"))

        make = open(os.path.join(basedir, "Makefile.sample"), "rt").read()
        self.failUnlessEqual(make, oldmake, "*should* rewrite Makefile.sample")

        samplecfg = open(os.path.join(basedir, "master.cfg.sample"),
                         "rt").read()
        self.failUnlessEqual(samplecfg, oldsamplecfg,
                             "*should* rewrite master.cfg.sample")


    def testSlave(self):
        basedir = "test_runner.slave"
        options = runner.SlaveOptions()
        options.parseOptions(["-q", basedir, "buildmaster:1234",
                              "botname", "passwd"])
        cwd = os.getcwd()
        runner.createSlave(options)
        os.chdir(cwd)

        tac = os.path.join(basedir, "buildbot.tac")
        self.failUnless(os.path.exists(tac))
        tacfile = open(tac,"rt").read()
        self.failUnlessIn("basedir", tacfile)
        self.failUnlessIn("host = 'buildmaster'", tacfile)
        self.failUnlessIn("port = 1234", tacfile)
        self.failUnlessIn("slavename = 'botname'", tacfile)
        self.failUnlessIn("passwd = 'passwd'", tacfile)
        self.failUnlessIn("keepalive = 600", tacfile)
        self.failUnlessIn("BuildSlave(host, port, slavename", tacfile)

        makefile = os.path.join(basedir, "Makefile.sample")
        self.failUnlessExists(makefile)

        self.failUnlessExists(os.path.join(basedir, "info", "admin"))
        self.failUnlessExists(os.path.join(basedir, "info", "host"))
        # edit one to make sure the later install doesn't change it
        f = open(os.path.join(basedir, "info", "admin"), "wt")
        f.write("updated@buildbot.example.org\n")
        f.close()

        # now verify that running it a second time (with the same options)
        # does the right thing: nothing changes
        runner.createSlave(options)
        os.chdir(cwd)

        self.failIfExists(os.path.join(basedir, "buildbot.tac.new"))
        admin = open(os.path.join(basedir, "info", "admin"), "rt").read()
        self.failUnlessEqual(admin, "updated@buildbot.example.org\n")


        # mutate Makefile.sample, since it should be rewritten
        oldmake = open(os.path.join(basedir, "Makefile.sample"), "rt").read()
        f = open(os.path.join(basedir, "Makefile.sample"), "wt")
        f.write(oldmake)
        f.write("# additional line added\n")
        f.close()
        oldtac = open(os.path.join(basedir, "buildbot.tac"), "rt").read()

        # now run it again (with different options)
        options = runner.SlaveOptions()
        options.parseOptions(["-q", "--keepalive", "30",
                              basedir, "buildmaster:9999",
                              "newbotname", "passwd"])
        runner.createSlave(options)
        os.chdir(cwd)

        tac = open(os.path.join(basedir, "buildbot.tac"), "rt").read()
        self.failUnlessEqual(tac, oldtac, "shouldn't change existing .tac")
        self.failUnlessExists(os.path.join(basedir, "buildbot.tac.new"))
        tacfile = open(os.path.join(basedir, "buildbot.tac.new"),"rt").read()
        self.failUnlessIn("basedir", tacfile)
        self.failUnlessIn("host = 'buildmaster'", tacfile)
        self.failUnlessIn("port = 9999", tacfile)
        self.failUnlessIn("slavename = 'newbotname'", tacfile)
        self.failUnlessIn("passwd = 'passwd'", tacfile)
        self.failUnlessIn("keepalive = 30", tacfile)
        self.failUnlessIn("BuildSlave(host, port, slavename", tacfile)

        make = open(os.path.join(basedir, "Makefile.sample"), "rt").read()
        self.failUnlessEqual(make, oldmake, "*should* rewrite Makefile.sample")

class Try(unittest.TestCase):
    # test some aspects of the 'buildbot try' command
    def makeOptions(self, contents):
        if os.path.exists(".buildbot"):
            shutil.rmtree(".buildbot")
        os.mkdir(".buildbot")
        open(os.path.join(".buildbot", "options"), "w").write(contents)

    def testGetopt1(self):
        opts = "try_connect = 'ssh'\n" + "try_builders = ['a']\n"
        self.makeOptions(opts)
        config = runner.TryOptions()
        config.parseOptions([])
        t = tryclient.Try(config)
        self.failUnlessEqual(t.connect, "ssh")
        self.failUnlessEqual(t.builderNames, ['a'])

    def testGetopt2(self):
        opts = ""
        self.makeOptions(opts)
        config = runner.TryOptions()
        config.parseOptions(['--connect=ssh', '--builder', 'a'])
        t = tryclient.Try(config)
        self.failUnlessEqual(t.connect, "ssh")
        self.failUnlessEqual(t.builderNames, ['a'])

    def testGetopt3(self):
        opts = ""
        self.makeOptions(opts)
        config = runner.TryOptions()
        config.parseOptions(['--connect=ssh',
                             '--builder', 'a', '--builder=b'])
        t = tryclient.Try(config)
        self.failUnlessEqual(t.connect, "ssh")
        self.failUnlessEqual(t.builderNames, ['a', 'b'])

    def testGetopt4(self):
        opts = "try_connect = 'ssh'\n" + "try_builders = ['a']\n"
        self.makeOptions(opts)
        config = runner.TryOptions()
        config.parseOptions(['--builder=b'])
        t = tryclient.Try(config)
        self.failUnlessEqual(t.connect, "ssh")
        self.failUnlessEqual(t.builderNames, ['b'])

    def testGetTopdir(self):
        os.mkdir("gettopdir")
        os.mkdir(os.path.join("gettopdir", "foo"))
        os.mkdir(os.path.join("gettopdir", "foo", "bar"))
        open(os.path.join("gettopdir", "1"),"w").write("1")
        open(os.path.join("gettopdir", "foo", "2"),"w").write("2")
        open(os.path.join("gettopdir", "foo", "bar", "3"),"w").write("3")

        target = os.path.abspath("gettopdir")
        t = tryclient.getTopdir("1", "gettopdir")
        self.failUnlessEqual(os.path.abspath(t), target)
        t = tryclient.getTopdir("1", os.path.join("gettopdir", "foo"))
        self.failUnlessEqual(os.path.abspath(t), target)
        t = tryclient.getTopdir("1", os.path.join("gettopdir", "foo", "bar"))
        self.failUnlessEqual(os.path.abspath(t), target)

        target = os.path.abspath(os.path.join("gettopdir", "foo"))
        t = tryclient.getTopdir("2", os.path.join("gettopdir", "foo"))
        self.failUnlessEqual(os.path.abspath(t), target)
        t = tryclient.getTopdir("2", os.path.join("gettopdir", "foo", "bar"))
        self.failUnlessEqual(os.path.abspath(t), target)

        target = os.path.abspath(os.path.join("gettopdir", "foo", "bar"))
        t = tryclient.getTopdir("3", os.path.join("gettopdir", "foo", "bar"))
        self.failUnlessEqual(os.path.abspath(t), target)

        nonexistent = "nonexistent\n29fis3kq\tBAR"
        # hopefully there won't be a real file with that name between here
        # and the filesystem root.
        self.failUnlessRaises(ValueError, tryclient.getTopdir, nonexistent)

