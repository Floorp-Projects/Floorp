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

    def __init__(self, host=None, marionetteHost=None, marionettePort=2828,
                 **kwargs):
 
        # (allowing marionneteHost to be specified seems a bit
        # counter-intuitive since we normally get it below from the ip
        # address, however we currently need it to be able to connect
        # via adb port forwarding and localhost)
        if marionetteHost:
            self.marionetteHost = marionetteHost
        elif host:
            self.marionetteHost = host
        self.marionettePort = marionettePort

    def cleanup(self):
        """
        If a user profile was setup on the device, restore it to the original.
        """
        if self.profileDir:
            self.restoreProfile()

    def waitForPort(self, timeout):
        """Waits for the marionette server to respond, until the timeout specified.

	:param timeout: Timeout parameter in seconds.
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

    def setupMarionette(self, scriptTimeout=60000):
        """
        Starts a marionette session.
        If no host was given at init, the ip of the device will be retrieved
        and networking will be established.
        """
        if not self.marionetteHost:
            self.setupDHCP()
            self.marionetteHost = self.getIP()
        if not self.marionette:
            self.marionette = Marionette(self.marionetteHost, self.marionettePort)
        if not self.marionette.session:
            self.waitForPort(30)
            self.marionette.start_session()

        self.marionette.set_script_timeout(scriptTimeout)

    def restartB2G(self):
        """
        Restarts the b2g process on the device.
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
        """Sets up the user profile on the device.

        :param prefs: String of user_prefs to add to the profile. Defaults to a standard b2g testing profile.
        """
        # currently we have no custom prefs to set (when bug 800138 is fixed,
        # we will probably want to enable marionette on an external ip by
        # default)
        if not prefs:
            prefs = ""

        #remove previous user.js if there is one
        if not self.profileDir:
            self.profileDir = tempfile.mkdtemp()
        our_userJS = os.path.join(self.profileDir, "user.js")
        if os.path.exists(our_userJS):
            os.remove(our_userJS)
        #copy profile
        try:
            self.getFile(self.userJS, our_userJS)
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

    def setupDHCP(self, interfaces=['eth0', 'wlan0']):
        """Sets up networking.

        :param interfaces: Network connection types to try. Defaults to eth0 and wlan0.
        """
        all_interfaces = [line.split()[0] for line in \
                          self.shellCheckOutput(['netcfg']).splitlines()[1:]]
        interfaces_to_try = filter(lambda i: i in interfaces, all_interfaces)

        tries = 5
        print "Setting up DHCP..."
        while tries > 0:
            print "attempts left: %d" % tries
            try:
                for interface in interfaces_to_try:
                    self.shellCheckOutput(['netcfg', interface, 'dhcp'],
                                          timeout=10)
                    if self.getIP(interfaces=[interface]):
                        return
            except DMError:
                pass
            time.sleep(1)
            tries -= 1
        raise DMError("Could not set up network connection")

    def restoreProfile(self):
        """
        Restores the original user profile on the device.
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

        :rtype: dictionary
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
