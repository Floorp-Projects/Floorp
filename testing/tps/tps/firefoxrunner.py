# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import copy
import httplib2
import os
import shutil

import mozinstall

from mozprofile import Profile
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
      
    def download_url(self, url, dest=None):
        h = httplib2.Http()
        resp, content = h.request(url, "GET")
        if dest == None:
            dest = os.path.basename(url)
    
        local = open(dest, 'wb')
        local.write(content)
        local.close()
        return dest
    
    def download_build(self, installdir='downloadedbuild', appname='firefox'):
        self.installdir = os.path.abspath(installdir)
        buildName = os.path.basename(self.url)
        pathToBuild = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   buildName)
    
        # delete the build if it already exists
        if os.access(pathToBuild, os.F_OK):
            os.remove(pathToBuild)
      
        # download the build
        print "downloading build"
        self.download_url(self.url, pathToBuild)
    
        # install the build
        print "installing %s" % pathToBuild
        shutil.rmtree(self.installdir, True)
        binary = mozinstall.install(src=pathToBuild, dest=self.installdir)
    
        # remove the downloaded archive
        os.remove(pathToBuild)
    
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
