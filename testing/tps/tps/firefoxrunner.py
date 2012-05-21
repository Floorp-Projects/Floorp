# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
