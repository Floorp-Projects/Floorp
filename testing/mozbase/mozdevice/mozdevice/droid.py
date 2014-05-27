# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import StringIO
import moznetwork
import re
import threading
import time

from Zeroconf import Zeroconf, ServiceBrowser
from devicemanager import ZeroconfListener
from devicemanagerADB import DeviceManagerADB
from devicemanagerSUT import DeviceManagerSUT
from devicemanager import DMError
from distutils.version import StrictVersion

class DroidMixin(object):
    """Mixin to extend DeviceManager with Android-specific functionality"""

    _stopApplicationNeedsRoot = True

    def _getExtraAmStartArgs(self):
        return []

    def launchApplication(self, appName, activityName, intent, url=None,
                          extras=None, wait=True, failIfRunning=True):
        """
        Launches an Android application

        :param appName: Name of application (e.g. `com.android.chrome`)
        :param activityName: Name of activity to launch (e.g. `.Main`)
        :param intent: Intent to launch application with
        :param url: URL to open
        :param extras: Dictionary of extra arguments to launch application with
        :param wait: If True, wait for application to start before returning
        :param failIfRunning: Raise an exception if instance of application is already running
        """

        # If failIfRunning is True, we throw an exception here. Only one
        # instance of an application can be running at once on Android,
        # starting a new instance may not be what we want depending on what
        # we want to do
        if failIfRunning and self.processExist(appName):
            raise DMError("Only one instance of an application may be running "
                          "at once")

        acmd = [ "am", "start" ] + self._getExtraAmStartArgs() + \
            ["-W" if wait else '', "-n", "%s/%s" % (appName, activityName)]

        if intent:
            acmd.extend(["-a", intent])

        if extras:
            for (key, val) in extras.iteritems():
                if type(val) is int:
                    extraTypeParam = "--ei"
                elif type(val) is bool:
                    extraTypeParam = "--ez"
                else:
                    extraTypeParam = "--es"
                acmd.extend([extraTypeParam, str(key), str(val)])

        if url:
            acmd.extend(["-d", url])

        # shell output not that interesting and debugging logs should already
        # show what's going on here... so just create an empty memory buffer
        # and ignore (except on error)
        shellOutput = StringIO.StringIO()
        if self.shell(acmd, shellOutput) == 0:
            return

        shellOutput.seek(0)
        raise DMError("Unable to launch application (shell output: '%s')" % shellOutput.read())

    def launchFennec(self, appName, intent="android.intent.action.VIEW",
                     mozEnv=None, extraArgs=None, url=None, wait=True,
                     failIfRunning=True):
        """
        Convenience method to launch Fennec on Android with various debugging
        arguments

        :param appName: Name of fennec application (e.g. `org.mozilla.fennec`)
        :param intent: Intent to launch application with
        :param mozEnv: Mozilla specific environment to pass into application
        :param extraArgs: Extra arguments to be parsed by fennec
        :param url: URL to open
        :param wait: If True, wait for application to start before returning
        :param failIfRunning: Raise an exception if instance of application is already running
        """
        extras = {}

        if mozEnv:
            # mozEnv is expected to be a dictionary of environment variables: Fennec
            # itself will set them when launched
            for (envCnt, (envkey, envval)) in enumerate(mozEnv.iteritems()):
                extras["env" + str(envCnt)] = envkey + "=" + envval

        # Additional command line arguments that fennec will read and use (e.g.
        # with a custom profile)
        if extraArgs:
            extras['args'] = " ".join(extraArgs)

        self.launchApplication(appName, ".App", intent, url=url, extras=extras,
                               wait=wait, failIfRunning=failIfRunning)

    def getInstalledApps(self):
        """
        Lists applications installed on this Android device

        Returns a list of application names in the form [ 'org.mozilla.fennec', ... ]
        """
        output = self.shellCheckOutput(["pm", "list", "packages", "-f"])
        apps = []
        for line in output.splitlines():
            # lines are of form: package:/system/app/qik-tmo.apk=com.qiktmobile.android
            apps.append(line.split('=')[1])

        return apps

    def stopApplication(self, appName):
        """
        Stops the specified application

        For Android 3.0+, we use the "am force-stop" to do this, which is
        reliable and does not require root. For earlier versions of Android,
        we simply try to manually kill the processes started by the app
        repeatedly until none is around any more. This is less reliable and
        does require root.

        :param appName: Name of application (e.g. `com.android.chrome`)
        """
        version = self.shellCheckOutput(["getprop", "ro.build.version.release"])
        if StrictVersion(version) >= StrictVersion('3.0'):
            self.shellCheckOutput([ "am", "force-stop", appName ], root=self._stopApplicationNeedsRoot)
        else:
            num_tries = 0
            max_tries = 5
            while self.processExist(appName):
                if num_tries > max_tries:
                    raise DMError("Couldn't successfully kill %s after %s "
                                  "tries" % (appName, max_tries))
                self.killProcess(appName)
                num_tries += 1

                # sleep for a short duration to make sure there are no
                # additional processes in the process of being launched
                # (this is not 100% guaranteed to work since it is inherently
                # racey, but it's the best we can do)
                time.sleep(1)

class DroidADB(DeviceManagerADB, DroidMixin):

    _stopApplicationNeedsRoot = False

    def getTopActivity(self):
        package = None
        data = self.shellCheckOutput(["dumpsys", "window", "windows"])
        # "dumpsys window windows" produces many lines of input. The top/foreground
        # activity is indicated by something like:
        #   mFocusedApp=AppWindowToken{483e6db0 token=HistoryRecord{484dcad8 com.mozilla.SUTAgentAndroid/.SUTAgentAndroid}}
        # or, on other devices:
        #   FocusedApplication: name='AppWindowToken{41a65340 token=ActivityRecord{418fbd68 org.mozilla.fennec_mozdev/.App}}', dispatchingTimeout=5000.000ms
        # Extract this line, ending in the forward slash:
        m = re.search('mFocusedApp(.+)/', data)
        if not m:
            m = re.search('FocusedApplication(.+)/', data)
        if m:
            line = m.group(0)
            # Extract package name: string of non-whitespace ending in forward slash
            m = re.search('(\S+)/$', line)
            if m:
                package = m.group(1)
        if not package:
            raise DMError("unable to find focused app")
        return package

class DroidSUT(DeviceManagerSUT, DroidMixin):

    def _getExtraAmStartArgs(self):
        # in versions of android in jellybean and beyond, the agent may run as
        # a different process than the one that started the app. In this case,
        # we need to get back the original user serial number and then pass
        # that to the 'am start' command line
        if not hasattr(self, '_userSerial'):
            infoDict = self.getInfo(directive="sutuserinfo")
            if infoDict.get('sutuserinfo') and \
                    len(infoDict['sutuserinfo']) > 0:
               userSerialString = infoDict['sutuserinfo'][0]
               # user serial always an integer, see: http://developer.android.com/reference/android/os/UserManager.html#getSerialNumberForUser%28android.os.UserHandle%29
               m = re.match('User Serial:([0-9]+)', userSerialString)
               if m:
                   self._userSerial = m.group(1)
               else:
                   self._userSerial = None
            else:
                self._userSerial = None

        if self._userSerial is not None:
            return [ "--user", self._userSerial ]

        return []

    def getTopActivity(self):
        return self._runCmds([{ 'cmd': "activity" }]).strip()

def DroidConnectByHWID(hwid, timeout=30, **kwargs):
    """Try to connect to the given device by waiting for it to show up using mDNS with the given timeout."""
    zc = Zeroconf(moznetwork.get_ip())

    evt = threading.Event()
    listener = ZeroconfListener(hwid, evt)
    sb = ServiceBrowser(zc, "_sutagent._tcp.local.", listener)
    foundIP = None
    if evt.wait(timeout):
        # we found the hwid
        foundIP = listener.ip
    sb.cancel()
    zc.close()

    if foundIP is not None:
        return DroidSUT(foundIP, **kwargs)
    print "Connected via SUT to %s [at %s]" % (hwid, foundIP)

    # try connecting via adb
    try:
        sut = DroidADB(deviceSerial=hwid, **kwargs)
    except:
        return None

    print "Connected via ADB to %s" % (hwid)
    return sut
