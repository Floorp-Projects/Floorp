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
# The Original Code is TPS.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Jonathan Griffin <jgriffin@mozilla.com>
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

import copy
import os
import shutil
import sys

from mozprocess.pid import get_pids
from mozprofile import Profile
from mozregression.mozInstall import MozInstaller
from mozregression.utils import download_url, get_platform
from mozrunner import FirefoxRunner

class TPSFirefoxRunner(object):

  PROCESS_TIMEOUT = 240

  def __init__(self, binary):
    if binary is not None and ('http://' in binary or 'ftp://' in binary):
      self.url = binary
      self.binary = None
    else:
      self.url = None
      self.binary = binary
    self.runner = None
    self.installdir = None

  def __del__(self):
    if self.installdir:
      shutil.rmtree(self.installdir, True)

  @property
  def names(self):
      if sys.platform == 'darwin':
          return ['firefox', 'minefield']
      if (sys.platform == 'linux2') or (sys.platform in ('sunos5', 'solaris')):
          return ['firefox', 'mozilla-firefox', 'minefield']
      if os.name == 'nt' or sys.platform == 'cygwin':
          return ['firefox']

  def download_build(self, installdir='downloadedbuild',
                     appname='firefox', macAppName='Minefield.app'):
    self.installdir = os.path.abspath(installdir)
    buildName = os.path.basename(self.url)
    pathToBuild = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               buildName)

    # delete the build if it already exists
    if os.access(pathToBuild, os.F_OK):
      os.remove(pathToBuild)

    # download the build
    print "downloading build"
    download_url(self.url, pathToBuild)

    # install the build
    print "installing %s" % pathToBuild
    shutil.rmtree(self.installdir, True)
    MozInstaller(src=pathToBuild, dest=self.installdir, dest_app=macAppName)

    # remove the downloaded archive
    os.remove(pathToBuild)

    # calculate path to binary
    platform = get_platform()
    if platform['name'] == 'Mac':
      binary = '%s/%s/Contents/MacOS/%s-bin' % (installdir,
                                                macAppName,
                                                appname)
    else:
      binary = '%s/%s/%s%s' % (installdir,
                               appname,
                               appname,
                               '.exe' if platform['name'] == 'Windows' else '')

    return binary

  def get_respository_info(self):
    """Read repository information from application.ini and platform.ini."""
    import ConfigParser

    config = ConfigParser.RawConfigParser()
    dirname = os.path.dirname(self.runner.binary)
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

  def run(self, profile=None, timeout=PROCESS_TIMEOUT, env=None, args=None):
    """Runs the given FirefoxRunner with the given Profile, waits
       for completion, then returns the process exit code
    """
    if profile is None:
      profile = Profile()
    self.profile = profile

    if self.binary is None and self.url:
      self.binary = self.download_build()

    if self.runner is None:
      self.runner = FirefoxRunner(self.profile, binary=self.binary)

    self.runner.profile = self.profile

    if env is not None:
      self.runner.env.update(env)

    if args is not None:
      self.runner.cmdargs = copy.copy(args)

    self.runner.start()

    status = self.runner.process_handler.waitForFinish(timeout=timeout)

    return status
