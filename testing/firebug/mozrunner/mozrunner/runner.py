# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

__all__ = ['Runner', 'ThunderbirdRunner', 'FirefoxRunner', 'create_runner', 'CLI', 'cli']

import os
import sys
import signal
import optparse
import ConfigParser

from utils import findInPath
from mozprocess import killableprocess
from mozprocess.pid import get_pids
from mozprofile import *

class Runner(object):
    """Handles all running operations. Finds bins, runs and kills the process."""

    def __init__(self, profile, binary=None, cmdargs=None, env=None, kp_kwargs=None):
        self.process_handler = None
        self.profile = profile
                 
        self.binary = self.__class__.get_binary(binary)

        if not os.path.exists(self.binary):
            raise Exception("Binary path does not exist "+self.binary)

        self.cmdargs = cmdargs or []
        _cmdargs = [i for i in self.cmdargs
                    if i != '-foreground']
        if len(_cmdargs) != len(self.cmdargs):
            # foreground should be last; see
            # - https://bugzilla.mozilla.org/show_bug.cgi?id=625614
            # - https://bugzilla.mozilla.org/show_bug.cgi?id=626826
            self.cmdargs = _cmdargs
            self.cmdargs.append('-foreground')

        if env is None:
            self.env = os.environ.copy()
            self.env.update({'MOZ_NO_REMOTE':'1',})
        else:
            self.env = env
        self.kp_kwargs = kp_kwargs or {}

    @classmethod
    def get_binary(cls, binary=None):
        """determine the binary"""
        if binary is None:
            return cls.find_binary()
        elif sys.platform == 'darwin' and binary.find('Contents/MacOS/') == -1:
            # TODO FIX ME!!!
            return os.path.join(binary, 'Contents/MacOS/%s-bin' % cls.names[0])
        else:
            return binary
        
    @classmethod
    def find_binary(cls):
        """Finds the binary for class names if one was not provided."""

        binary = None
        if sys.platform in ('linux2', 'sunos5', 'solaris'):
            for name in reversed(cls.names):
                binary = findInPath(name)
        elif os.name == 'nt' or sys.platform == 'cygwin':

            # find the default executable from the windows registry
            try:
                # assumes cls.app_name is defined, as it should be for
                # implementors
                import _winreg
                app_key = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, r"Software\Mozilla\Mozilla %s" % cls.app_name)
                version, _type = _winreg.QueryValueEx(app_key, "CurrentVersion")
                version_key = _winreg.OpenKey(app_key, version + r"\Main")
                path, _ = _winreg.QueryValueEx(version_key, "PathToExe")
                return path
            except: # XXX not sure what type of exception this should be
                pass

            # search for the binary in the path            
            for name in reversed(cls.names):
                binary = findInPath(name)
                if sys.platform == 'cygwin':
                    program_files = os.environ['PROGRAMFILES']
                else:
                    program_files = os.environ['ProgramFiles']

                if binary is None:
                    for bin in [(program_files, 'Mozilla Firefox', 'firefox.exe'),
                                (os.environ.get("ProgramFiles(x86)"),'Mozilla Firefox', 'firefox.exe'),
                                (program_files,'Minefield', 'firefox.exe'),
                                (os.environ.get("ProgramFiles(x86)"),'Minefield', 'firefox.exe')
                                ]:
                        path = os.path.join(*bin)
                        if os.path.isfile(path):
                            binary = path
                            break
        elif sys.platform == 'darwin':
            for name in reversed(cls.names):
                appdir = os.path.join('Applications', name.capitalize()+'.app')
                if os.path.isdir(os.path.join(os.path.expanduser('~/'), appdir)):
                    binary = os.path.join(os.path.expanduser('~/'), appdir,
                                          'Contents/MacOS/'+name+'-bin')
                elif os.path.isdir('/'+appdir):
                    binary = os.path.join("/"+appdir, 'Contents/MacOS/'+name+'-bin')

                if binary is not None:
                    if not os.path.isfile(binary):
                        binary = binary.replace(name+'-bin', 'firefox-bin')
                    if not os.path.isfile(binary):
                        binary = None
        if binary is None:
            raise Exception('Mozrunner could not locate your binary, you will need to set it.')
        return binary

    @property
    def command(self):
        """Returns the command list to run."""
        return [self.binary, '-profile', self.profile.profile]

    def get_repositoryInfo(self):
        """Read repository information from application.ini and platform.ini."""
        # TODO: I think we should keep this, but I think Jeff's patch moves it to the top of the fileimport ConfigParser

        config = ConfigParser.RawConfigParser()
        dirname = os.path.dirname(self.binary)
        repository = { }

        for file, section in [('application', 'App'), ('platform', 'Build')]:
            config.read(os.path.join(dirname, '%s.ini' % file))

            for key, id in [('SourceRepository', 'repository'),
                           ('SourceStamp', 'changeset')]:
                try:
                    repository['%s_%s' % (file, id)] = config.get(section, key);
                except:
                    repository['%s_%s' % (file, id)] = None

        return repository

    def start(self):
        """Run self.command in the proper environment."""
        self.process_handler = killableprocess.runCommand(self.command+self.cmdargs, env=self.env, **self.kp_kwargs)

    def wait(self, timeout=None):
        """Wait for the browser to exit."""
        self.process_handler.wait(timeout=timeout)

        if sys.platform != 'win32':
            for name in self.names:
                for pid in get_pids(name, self.process_handler.pid):
                    self.process_handler.pid = pid
                    self.process_handler.wait(timeout=timeout)

    def stop(self):
        """Kill the app"""
        if self.process_handler is None:
            return
        
        if sys.platform != 'win32':
            self.process_handler.kill()
            for name in self.names:
                for pid in get_pids(name, self.process_handler.pid):
                    self.process_handler.pid = pid
                    self.process_handler.kill()
        else:
            try:
                self.process_handler.kill(group=True)
            except Exception, e:
                raise Exception('Cannot kill process, '+type(e).__name__+' '+e.message)

    def reset(self):
        """
        reset the runner between runs
        currently, only resets the profile, but probably should do more
        """
        self.profile.reset()

    def cleanup(self):
        self.stop()
        self.profile.cleanup()

    __del__ = cleanup


class FirefoxRunner(Runner):
    """Specialized Runner subclass for running Firefox."""

    app_name = 'Firefox'
    profile_class = FirefoxProfile

    if sys.platform == 'darwin':
        names = ['firefox', 'minefield', 'shiretoko']
    elif (sys.platform == 'linux2') or (sys.platform in ('sunos5', 'solaris')):
        names = ['firefox', 'mozilla-firefox', 'iceweasel']
    elif os.name == 'nt' or sys.platform == 'cygwin':
        names =['firefox']
    else:
        raise AssertionError("I don't know what platform you're on")

class ThunderbirdRunner(Runner):
    """Specialized Runner subclass for running Thunderbird"""
    app_name = 'Thunderbird'

    names = ["thunderbird", "shredder"]

def create_runner(profile_class, runner_class,
                  binary=None, profile_args=None, runner_args=None):
    """Get the runner object, a not-very-abstract factory"""
    profile_args = profile_args or {}
    runner_args = runner_args or {}
    profile = profile_class(**profile_args)
    binary = runner_class.get_binary(binary)
    runner = runner_class(binary=binary,
                          profile=profile,
                          **runner_args)
    return runner    

class CLI(object):
    """Command line interface."""

    module = "mozrunner"

    def __init__(self, args=sys.argv[1:]):
        """
        Setup command line parser and parse arguments
        - args : command line arguments
        """

        self.metadata = self.get_metadata_from_egg()
        self.parser = optparse.OptionParser(version="%prog " + self.metadata["Version"])
        self.add_options(self.parser)
        (self.options, self.args) = self.parser.parse_args(args)

        if self.options.info:
            self.print_metadata()
            sys.exit(0)

        # choose appropriate runner and profile classes
        if self.options.app == 'firefox':
            self.runner_class = FirefoxRunner
            self.profile_class = FirefoxProfile
        elif self.options.app == 'thunderbird':
            self.runner_class = ThunderbirdRunner
            self.profile_class = ThunderbirdProfile
        else:
            self.parser.error('Application "%s" unknown (should be one of "firefox" or "thunderbird"' % self.options.app)

    def add_options(self, parser):
        """add options to the parser"""
        
        parser.add_option('-b', "--binary",
                          dest="binary", help="Binary path.",
                          metavar=None, default=None)
        
        parser.add_option('-p', "--profile",
                         dest="profile", help="Profile path.",
                         metavar=None, default=None)
        
        parser.add_option('-a', "--addon", dest="addons",
                         action='append',
                         help="Addons paths to install",
                         metavar=None, default=[])
        
        parser.add_option("--info", dest="info", default=False,
                          action="store_true",
                          help="Print module information")
        parser.add_option('--app', dest='app', default='firefox',
                          help="Application to use [DEFAULT: %default]")
        parser.add_option('--app-arg', dest='appArgs',
                          default=[], action='append',
                          help="provides an argument to the test application")

    ### methods regarding introspecting data            
    def get_metadata_from_egg(self):
        import pkg_resources
        ret = {}
        dist = pkg_resources.get_distribution(self.module)
        if dist.has_metadata("PKG-INFO"):
            for line in dist.get_metadata_lines("PKG-INFO"):
                key, value = line.split(':', 1)
                ret[key] = value
        if dist.has_metadata("requires.txt"):
            ret["Dependencies"] = "\n" + dist.get_metadata("requires.txt")    
        return ret
        
    def print_metadata(self, data=("Name", "Version", "Summary", "Home-page", 
                                   "Author", "Author-email", "License", "Platform", "Dependencies")):
        for key in data:
            if key in self.metadata:
                print key + ": " + self.metadata[key]

    ### methods for running
    def profile_args(self):
        """arguments to instantiate the profile class"""
        return dict(profile=self.options.profile,
                    addons=self.options.addons)

    def command_args(self):
        """additional arguments for the mozilla application"""
        return self.options.appArgs

    def runner_args(self):
        """arguments to instantiate the runner class"""
        return dict(cmdargs=self.command_args())

    def create_runner(self):
        return create_runner(self.profile_class,
                             self.runner_class,
                             self.options.binary,
                             self.profile_args(),
                             self.runner_args())

    def run(self):
        runner = self.create_runner()
        self.start(runner)
        # XXX should be runner.cleanup,
        # and other runner cleanup code should go in there
        runner.profile.cleanup()

    def start(self, runner):
        """Starts the runner and waits for Firefox to exitor Keyboard Interrupt.
        Shoule be overwritten to provide custom running of the runner instance."""
        runner.start()
        print 'Started:', ' '.join(runner.command)
        try:
            runner.wait()
        except KeyboardInterrupt:
            runner.stop()


def cli(args=sys.argv[1:]):
    CLI(args).run()

if __name__ == '__main__':
    cli()
