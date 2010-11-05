# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Corporation Code.
#
# The Initial Developer of the Original Code is
# Mikeal Rogers.
# Portions created by the Initial Developer are Copyright (C) 2008-2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Mikeal Rogers <mikeal.rogers@gmail.com>
#  Clint Talbert <ctalbert@mozilla.com>
#  Henrik Skupin <hskupin@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

import os
import sys
import copy
import tempfile
import signal
import commands
import zipfile
import optparse
import killableprocess
import subprocess
import platform

from distutils import dir_util
from time import sleep
from xml.dom import minidom

# conditional (version-dependent) imports
try:
    import simplejson
except ImportError:
    import json as simplejson

import logging
logger = logging.getLogger(__name__)

# Use dir_util for copy/rm operations because shutil is all kinds of broken
copytree = dir_util.copy_tree
rmtree = dir_util.remove_tree

def findInPath(fileName, path=os.environ['PATH']):
    dirs = path.split(os.pathsep)
    for dir in dirs:
        if os.path.isfile(os.path.join(dir, fileName)):
            return os.path.join(dir, fileName)
        if os.name == 'nt' or sys.platform == 'cygwin':
            if os.path.isfile(os.path.join(dir, fileName + ".exe")):
                return os.path.join(dir, fileName + ".exe")
    return None

stdout = sys.stdout
stderr = sys.stderr
stdin = sys.stdin

def run_command(cmd, env=None, **kwargs):
    """Run the given command in killable process."""
    killable_kwargs = {'stdout':stdout ,'stderr':stderr, 'stdin':stdin}
    killable_kwargs.update(kwargs)

    if sys.platform != "win32":
        return killableprocess.Popen(cmd, preexec_fn=lambda : os.setpgid(0, 0),
                                     env=env, **killable_kwargs)
    else:
        return killableprocess.Popen(cmd, env=env, **killable_kwargs)

def getoutput(l):
    tmp = tempfile.mktemp()
    x = open(tmp, 'w')
    subprocess.call(l, stdout=x, stderr=x)
    x.close(); x = open(tmp, 'r')
    r = x.read() ; x.close()
    os.remove(tmp)
    return r

def get_pids(name, minimun_pid=0):
    """Get all the pids matching name, exclude any pids below minimum_pid."""
    if os.name == 'nt' or sys.platform == 'cygwin':
        import wpk

        pids = wpk.get_pids(name)

    else:
        # get_pids_cmd = ['ps', 'ax']
        # h = killableprocess.runCommand(get_pids_cmd, stdout=subprocess.PIPE, universal_newlines=True)
        # h.wait(group=False)
        # data = h.stdout.readlines()
        data = getoutput(['ps', 'ax']).splitlines()
        pids = [int(line.split()[0]) for line in data if line.find(name) is not -1]

    matching_pids = [m for m in pids if m > minimun_pid]
    return matching_pids

def kill_process_by_name(name):
    """Find and kill all processes containing a certain name"""

    pids = get_pids(name)

    if os.name == 'nt' or sys.platform == 'cygwin':
        for p in pids:
            import wpk

            wpk.kill_pid(p)

    else:
        for pid in pids:
            try:
                os.kill(pid, signal.SIGTERM)
            except OSError: pass
            sleep(.5)
            if len(get_pids(name)) is not 0:
                try:
                    os.kill(pid, signal.SIGKILL)
                except OSError: pass
                sleep(.5)
                if len(get_pids(name)) is not 0:
                    logger.error('Could not kill process')

def makedirs(name):

    head, tail = os.path.split(name)
    if not tail:
        head, tail = os.path.split(head)
    if head and tail and not os.path.exists(head):
        try:
            makedirs(head)
        except OSError, e:
            pass
        if tail == os.curdir:           # xxx/newdir/. exists if xxx/newdir exists
            return
    try:
        os.mkdir(name)
    except:
        pass

class Profile(object):
    """Handles all operations regarding profile. Created new profiles, installs extensions,
    sets preferences and handles cleanup."""

    def __init__(self, binary=None, profile=None, addons=None,
                 preferences=None):

        self.binary = binary

        self.create_new = not(bool(profile))
        if profile:
            self.profile = profile
        else:
            self.profile = self.create_new_profile(self.binary)

        self.addons_installed = []
        self.addons = addons or []

        ### set preferences from class preferences
        preferences = preferences or {}
        if hasattr(self.__class__, 'preferences'):
            self.preferences = self.__class__.preferences.copy()
        else:
            self.preferences = {}
        self.preferences.update(preferences)

        for addon in self.addons:
            self.install_addon(addon)

        self.set_preferences(self.preferences)

    def create_new_profile(self, binary):
        """Create a new clean profile in tmp which is a simple empty folder"""
        profile = tempfile.mkdtemp(suffix='.mozrunner')
        return profile

    ### methods related to addons

    @classmethod
    def addon_id(self, addon_path):
        """
        return the id for a given addon, or None if not found
        - addon_path : path to the addon directory
        """
        
        def find_id(desc):
            """finds the addon id give its description"""
            
            addon_id = None
            for elem in desc:
                apps = elem.getElementsByTagName('em:targetApplication')
                if apps:
                    for app in apps:
                        # remove targetApplication nodes, they contain id's we aren't interested in
                        elem.removeChild(app)
                    if elem.getElementsByTagName('em:id'):
                        addon_id = str(elem.getElementsByTagName('em:id')[0].firstChild.data)
                    elif elem.hasAttribute('em:id'):
                        addon_id = str(elem.getAttribute('em:id'))
            return addon_id

        doc = minidom.parse(os.path.join(addon_path, 'install.rdf')) 

        for tag in 'Description', 'RDF:Description':
            desc = doc.getElementsByTagName(tag)
            addon_id = find_id(desc)
            if addon_id:
                return addon_id


    def install_addon(self, path):
        """Installs the given addon or directory of addons in the profile."""
        
        # if the addon is a directory, install all addons in it
        addons = [path]
        if not path.endswith('.xpi') and not os.path.exists(os.path.join(path, 'install.rdf')):
            addons = [os.path.join(path, x) for x in os.listdir(path)]

        # install each addon
        for addon in addons:

            # if the addon is an .xpi, uncompress it to a temporary directory
            if addon.endswith('.xpi'):
                tmpdir = tempfile.mkdtemp(suffix = "." + os.path.split(addon)[-1])
                compressed_file = zipfile.ZipFile(addon, "r")
                for name in compressed_file.namelist():
                    if name.endswith('/'):
                        makedirs(os.path.join(tmpdir, name))
                    else:
                        if not os.path.isdir(os.path.dirname(os.path.join(tmpdir, name))):
                            makedirs(os.path.dirname(os.path.join(tmpdir, name)))
                        data = compressed_file.read(name)
                        f = open(os.path.join(tmpdir, name), 'wb')
                        f.write(data) ; f.close()
                addon = tmpdir

            # determine the addon id
            addon_id = Profile.addon_id(addon)
            assert addon_id is not None, "The addon id could not be found: %s" % addon

            # copy the addon to the profile
            addon_path = os.path.join(self.profile, 'extensions', addon_id)
            copytree(addon, addon_path, preserve_symlinks=1)
            self.addons_installed.append(addon_path)

    def clean_addons(self):
        """Cleans up addons in the profile."""
        for addon in self.addons_installed:
            if os.path.isdir(addon):
                rmtree(addon)

    ### methods related to preferences

    def set_preferences(self, preferences):
        """Adds preferences dict to profile preferences"""

        prefs_file = os.path.join(self.profile, 'user.js')

        # Ensure that the file exists first otherwise create an empty file
        if os.path.isfile(prefs_file):
            f = open(prefs_file, 'a+')
        else:
            f = open(prefs_file, 'w')

        f.write('\n#MozRunner Prefs Start\n')

        pref_lines = ['user_pref(%s, %s);' %
                      (simplejson.dumps(k), simplejson.dumps(v) ) for k, v in
                       preferences.items()]
        for line in pref_lines:
            f.write(line+'\n')
        f.write('#MozRunner Prefs End\n')
        f.flush() ; f.close()

    def clean_preferences(self):
        """Removed preferences added by mozrunner."""
        lines = open(os.path.join(self.profile, 'user.js'), 'r').read().splitlines()
        s = lines.index('#MozRunner Prefs Start') ; e = lines.index('#MozRunner Prefs End')
        cleaned_prefs = '\n'.join(lines[:s] + lines[e+1:])
        f = open(os.path.join(self.profile, 'user.js'), 'w')
        f.write(cleaned_prefs) ; f.flush() ; f.close()

    ### cleanup 

    def cleanup(self):
        """Cleanup operations on the profile."""
        if self.create_new:
            rmtree(self.profile)
        else:
            self.clean_preferences()
            self.clean_addons()

class FirefoxProfile(Profile):
    """Specialized Profile subclass for Firefox"""
    preferences = {# Don't automatically update the application
                   'app.update.enabled' : False,
                   # Don't restore the last open set of tabs if the browser has crashed
                   'browser.sessionstore.resume_from_crash': False,
                   # Don't check for the default web browser
                   'browser.shell.checkDefaultBrowser' : False,
                   # Don't warn on exit when multiple tabs are open
                   'browser.tabs.warnOnClose' : False,
                   # Don't warn when exiting the browser
                   'browser.warnOnQuit': False,
                   # Only install add-ons from the profile and the app folder
                   'extensions.enabledScopes' : 5,
                   # Don't automatically update add-ons
                   'extensions.update.enabled'    : False,
                   # Don't open a dialog to show available add-on updates
                   'extensions.update.notifyUser' : False,
                   }

    @property
    def names(self):
        if sys.platform == 'darwin':
            return ['firefox', 'minefield', 'shiretoko']
        if (sys.platform == 'linux2') or (sys.platform in ('sunos5', 'solaris')):
            return ['firefox', 'mozilla-firefox', 'iceweasel']
        if os.name == 'nt' or sys.platform == 'cygwin':
            return ['firefox']

class ThunderbirdProfile(Profile):
    preferences = {'extensions.update.enabled'    : False,
                   'extensions.update.notifyUser' : False,
                   'browser.shell.checkDefaultBrowser' : False,
                   'browser.tabs.warnOnClose' : False,
                   'browser.warnOnQuit': False,
                   'browser.sessionstore.resume_from_crash': False,
                   }
    names = ["thunderbird", "shredder"]


class Runner(object):
    """Handles all running operations. Finds bins, runs and kills the process."""

    def __init__(self, binary=None, profile=None, cmdargs=[], env=None,
                 aggressively_kill=['crashreporter'], kp_kwargs={}):
        if binary is None:
            self.binary = self.find_binary()
        elif sys.platform == 'darwin' and binary.find('Contents/MacOS/') == -1:
            self.binary = os.path.join(binary, 'Contents/MacOS/%s-bin' % self.names[0])
        else:
            self.binary = binary

        if not os.path.exists(self.binary):
            raise Exception("Binary path does not exist "+self.binary)

        if sys.platform == 'linux2' and self.binary.endswith('-bin'):
            dirname = os.path.dirname(self.binary)
            if os.environ.get('LD_LIBRARY_PATH', None):
                os.environ['LD_LIBRARY_PATH'] = '%s:%s' % (os.environ['LD_LIBRARY_PATH'], dirname)
            else:
                os.environ['LD_LIBRARY_PATH'] = dirname

        self.profile = profile

        self.cmdargs = cmdargs
        if env is None:
            self.env = copy.copy(os.environ)
            self.env.update({'MOZ_NO_REMOTE':"1",})
        else:
            self.env = env
        self.aggressively_kill = aggressively_kill
        self.kp_kwargs = kp_kwargs

    def find_binary(self):
        """Finds the binary for self.names if one was not provided."""
        binary = None
        if sys.platform in ('linux2', 'sunos5', 'solaris'):
            for name in reversed(self.names):
                binary = findInPath(name)
        elif os.name == 'nt' or sys.platform == 'cygwin':

            # find the default executable from the windows registry
            try:
                # assumes self.app_name is defined, as it should be for
                # implementors
                import _winreg
                app_key = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, r"Software\Mozilla\Mozilla %s" % self.app_name)
                version, _type = _winreg.QueryValueEx(app_key, "CurrentVersion")
                version_key = _winreg.OpenKey(app_key, version + r"\Main")
                path, _ = _winreg.QueryValueEx(version_key, "PathToExe")
                return path
            except: # XXX not sure what type of exception this should be
                pass

            # search for the binary in the path            
            for name in reversed(self.names):
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
            for name in reversed(self.names):
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
        cmd = [self.binary, '-profile', self.profile.profile]
        # On i386 OS X machines, i386+x86_64 universal binaries need to be told
        # to run as i386 binaries.  If we're not running a i386+x86_64 universal
        # binary, then this command modification is harmless.
        if sys.platform == 'darwin':
            if hasattr(platform, 'architecture') and platform.architecture()[0] == '32bit':
                cmd = ['arch', '-i386'] + cmd
        return cmd

    def get_repositoryInfo(self):
        """Read repository information from application.ini and platform.ini."""
        import ConfigParser

        config = ConfigParser.RawConfigParser()
        dirname = os.path.dirname(self.binary)
        repository = { }

        for entry in [['application', 'App'], ['platform', 'Build']]:
            (file, section) = entry
            config.read(os.path.join(dirname, '%s.ini' % file))

            for entry in [['SourceRepository', 'repository'], ['SourceStamp', 'changeset']]:
                (key, id) = entry

                try:
                    repository['%s_%s' % (file, id)] = config.get(section, key);
                except:
                    repository['%s_%s' % (file, id)] = None

        return repository

    def start(self):
        """Run self.command in the proper environment."""
        if self.profile is None:
            self.profile = self.profile_class()
        self.process_handler = run_command(self.command+self.cmdargs, self.env, **self.kp_kwargs)

    def wait(self, timeout=None):
        """Wait for the browser to exit."""
        self.process_handler.wait(timeout=timeout)

        if sys.platform != 'win32':
            for name in self.names:
                for pid in get_pids(name, self.process_handler.pid):
                    self.process_handler.pid = pid
                    self.process_handler.wait(timeout=timeout)

    def kill(self, kill_signal=signal.SIGTERM):
        """Kill the browser"""
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
                logger.error('Cannot kill process, '+type(e).__name__+' '+e.message)

        for name in self.aggressively_kill:
            kill_process_by_name(name)

    def stop(self):
        self.kill()

class FirefoxRunner(Runner):
    """Specialized Runner subclass for running Firefox."""

    app_name = 'Firefox'
    profile_class = FirefoxProfile

    @property
    def names(self):
        if sys.platform == 'darwin':
            return ['firefox', 'minefield', 'shiretoko']
        if (sys.platform == 'linux2') or (sys.platform in ('sunos5', 'solaris')):
            return ['firefox', 'mozilla-firefox', 'iceweasel']
        if os.name == 'nt' or sys.platform == 'cygwin':
            return ['firefox']

class ThunderbirdRunner(Runner):
    """Specialized Runner subclass for running Thunderbird"""

    app_name = 'Thunderbird'
    profile_class = ThunderbirdProfile

    names = ["thunderbird", "shredder"]

class CLI(object):
    """Command line interface."""

    runner_class = FirefoxRunner
    profile_class = FirefoxProfile
    module = "mozrunner"

    parser_options = {("-b", "--binary",): dict(dest="binary", help="Binary path.",
                                                metavar=None, default=None),
                      ('-p', "--profile",): dict(dest="profile", help="Profile path.",
                                                 metavar=None, default=None),
                      ('-a', "--addons",): dict(dest="addons", 
                                                help="Addons paths to install.",
                                                metavar=None, default=None),
                      ("--info",): dict(dest="info", default=False,
                                        action="store_true",
                                        help="Print module information")
                     }

    def __init__(self):
        """ Setup command line parser and parse arguments """
        self.metadata = self.get_metadata_from_egg()
        self.parser = optparse.OptionParser(version="%prog " + self.metadata["Version"])
        for names, opts in self.parser_options.items():
            self.parser.add_option(*names, **opts)
        (self.options, self.args) = self.parser.parse_args()

        if self.options.info:
            self.print_metadata()
            sys.exit(0)
            
        # XXX should use action='append' instead of rolling our own
        try:
            self.addons = self.options.addons.split(',')
        except:
            self.addons = []
            
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

    def create_runner(self):
        """ Get the runner object """
        runner = self.get_runner(binary=self.options.binary)
        profile = self.get_profile(binary=runner.binary,
                                   profile=self.options.profile,
                                   addons=self.addons)
        runner.profile = profile
        return runner

    def get_runner(self, binary=None, profile=None):
        """Returns the runner instance for the given command line binary argument
        the profile instance returned from self.get_profile()."""
        return self.runner_class(binary, profile)

    def get_profile(self, binary=None, profile=None, addons=None, preferences=None):
        """Returns the profile instance for the given command line arguments."""
        addons = addons or []
        preferences = preferences or {}
        return self.profile_class(binary, profile, addons, preferences)

    def run(self):
        runner = self.create_runner()
        self.start(runner)
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


def cli():
    CLI().run()

def print_addon_ids(args=sys.argv[1:]):
    """print addon ids for testing"""
    for arg in args:
        print Profile.addon_id(arg)
    
    
