# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import with_statement
import datetime
import os
import re
import tempfile
import time
import shutil
import socket
import subprocess

from marionette import Marionette
from mozdevice import DeviceManagerADB, DeviceManagerSUT, DMError

class B2GMixin(object):
    profileDir = None
    userJS = "/data/local/user.js"
    marionette = None

    def __init__(self, host=None, marionette_port=2828, **kwargs):
        self.marionetteHost = host
        self.marionettePort = marionette_port

    def cleanup(self):
        if self.profileDir:
            self.restoreProfile()

    def waitForPort(self, timeout):
        """
        Wait for the marionette server to respond.
        Timeout parameter is in seconds
        """
        print "waiting for port"
        starttime = datetime.datetime.now()
        while datetime.datetime.now() - starttime < datetime.timedelta(seconds=timeout):
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                print "trying %s %s" % (self.marionettePort, self.marionetteHost)
                sock.connect((self.marionetteHost, self.marionettePort))
                data = sock.recv(16)
                sock.close()
                if '"from"' in data:
                    return True
            except socket.error:
                pass
            except Exception as e:
                raise DMError("Could not connect to marionette: %s" % e)
            time.sleep(1)
        raise DMError("Could not communicate with Marionette port")

    def setupMarionette(self):
        """
        Start a marionette session.
        If no host is given, then this will get the ip
        of the device, and set up networking if needed.
        """
        if not self.marionetteHost:
            self.setupDHCP()
            self.marionetteHost = self.getIP()
        if not self.marionette:
            self.marionette = Marionette(self.marionetteHost, self.marionettePort)
        if not self.marionette.session:
            self.waitForPort(30)
            self.marionette.start_session()

    def restartB2G(self):
        """
        Restarts the b2g process on the device
        """
        #restart b2g so we start with a clean slate
        if self.marionette and self.marionette.session:
            self.marionette.delete_session()
        self.shellCheckOutput(['stop', 'b2g'])
        # Wait for a bit to make sure B2G has completely shut down.
        tries = 10
        while "b2g" in self.shellCheckOutput(['ps', 'b2g']) and tries > 0:
            tries -= 1
            time.sleep(1)
        if tries == 0:
            raise DMError("Could not kill b2g process")
        self.shellCheckOutput(['start', 'b2g'])

    def setupProfile(self, prefs=None):
        """
        Sets up the user profile on the device,
        The 'prefs' is a string of user_prefs to add to the profile.
        If it is not set, it will default to a standard b2g testing profile.
        """
        if not prefs:
            prefs = """
user_pref("power.screen.timeout", 999999);
user_pref("devtools.debugger.force-local", false);
            """
        #remove previous user.js if there is one
        if not self.profileDir:
            self.profileDir = tempfile.mkdtemp()
        our_userJS = os.path.join(self.profileDir, "user.js")
        if os.path.exists(our_userJS):
            os.remove(our_userJS)
        #copy profile
        try:
            output = self.getFile(self.userJS, our_userJS)
        except subprocess.CalledProcessError:
            pass
        #if we successfully copied the profile, make a backup of the file
        if os.path.exists(our_userJS):
            self.shellCheckOutput(['dd', 'if=%s' % self.userJS, 'of=%s.orig' % self.userJS])
        with open(our_userJS, 'a') as user_file:
            user_file.write("%s" % prefs)
        self.pushFile(our_userJS, self.userJS)
        self.restartB2G()
        self.setupMarionette()

    def setupDHCP(self, conn_type='eth0'):
        """
        Sets up networking.

        If conn_type is not set, it will assume eth0.
        """
        tries = 5
        while tries > 0:
            print "attempts left: %d" % tries
            try:
                self.shellCheckOutput(['netcfg', conn_type, 'dhcp'], timeout=10)
                if self.getIP():
                    return
            except DMError:
                pass
            tries = tries - 1
        raise DMError("Could not set up network connection")

    def restoreProfile(self):
        """
        Restores the original profile
        """
        if not self.profileDir:
            raise DMError("There is no profile to restore")
        #if we successfully copied the profile, make a backup of the file
        our_userJS = os.path.join(self.profileDir, "user.js")
        if os.path.exists(our_userJS):
            self.shellCheckOutput(['dd', 'if=%s.orig' % self.userJS, 'of=%s' % self.userJS])
        shutil.rmtree(self.profileDir)
        self.profileDir = None

    def getAppInfo(self):
        """
        Returns the appinfo, with an additional "date" key.
        """
        if not self.marionette or not self.marionette.session:
            self.setupMarionette()
        self.marionette.set_context("chrome")
        appinfo = self.marionette.execute_script("""
                                var appInfo = Components.classes["@mozilla.org/xre/app-info;1"]
                                .getService(Components.interfaces.nsIXULAppInfo);
                                return appInfo;
                                """)
        (year, month, day) = (appinfo["appBuildID"][0:4], appinfo["appBuildID"][4:6], appinfo["appBuildID"][6:8])
        appinfo['date'] =  "%s-%s-%s" % (year, month, day)
        return appinfo

class DeviceADB(DeviceManagerADB, B2GMixin):
    def __init__(self, **kwargs):
        DeviceManagerADB.__init__(self, **kwargs)
        B2GMixin.__init__(self, **kwargs)

class DeviceSUT(DeviceManagerSUT, B2GMixin):
    def __init__(self, **kwargs):
        DeviceManagerSUT.__init__(self, **kwargs)
        B2GMixin.__init__(self, **kwargs)
