#! /usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Script name:   mouse_and_screen_resolution.py
# Purpose:       Sets mouse position and screen resolution for Windows 7 32-bit slaves
# Author(s):     Zambrano Gasparnian, Armen <armenzg@mozilla.com>
# Target:        Python 2.5 or newer
#
from optparse import OptionParser
from ctypes import windll, Structure, c_ulong, byref
try:
    import json
except:
    import simplejson as json
import os
import sys
import urllib2
import socket
import platform
import time

default_screen_resolution = {"x": 1024, "y": 768}
default_mouse_position = {"x": 1010, "y": 10}

def wfetch(url, retries=5):
    while True:
        try:
            return urllib2.urlopen(url, timeout=30).read()
        except urllib2.HTTPError, e:
            print("Failed to fetch '%s': %s" % (url, str(e)))
        except urllib2.URLError, e:
            print("Failed to fetch '%s': %s" % (url, str(e)))
        except socket.timeout, e:
            print("Time out accessing %s: %s" % (url, str(e)))
        except socket.error, e:
            print("Socket error when accessing %s: %s" % (url, str(e)))
        if retries < 0:
            raise Exception("Could not fetch url '%s'" % url)
        retries -= 1
        print("Retrying")
        time.sleep(60)

def main():

    if not (platform.version().startswith('6.1.760') and not 'PROGRAMFILES(X86)' in os.environ):
        # We only want to run this for Windows 7 32-bit
        print "INFO: This script was written to be used with Windows 7 32-bit machines."
        return 0

    parser = OptionParser()
    parser.add_option(
        "--configuration-url", dest="configuration_url", type="string",
        help="Specifies the url of the configuration file.")
    parser.add_option(
        "--configuration-file", dest="configuration_file", type="string",
        help="Specifies the path to the configuration file.")
    (options, args) = parser.parse_args()

    if (options.configuration_url == None and
        options.configuration_file == None):
        print "You must specify --configuration-url or --configuration-file."
        return 1

    if options.configuration_file:
        with open(options.configuration_file) as f:
            conf_dict = json.load(f)
        new_screen_resolution = conf_dict["win7"]["screen_resolution"]
        new_mouse_position = conf_dict["win7"]["mouse_position"]
    else:
        try:
            conf_dict = json.loads(wfetch(options.configuration_url))
            new_screen_resolution = conf_dict["win7"]["screen_resolution"]
            new_mouse_position = conf_dict["win7"]["mouse_position"]
        except urllib2.HTTPError, e:
            print "This branch does not seem to have the configuration file %s" % str(e)
            print "Let's fail over to 1024x768."
            new_screen_resolution = default_screen_resolution
            new_mouse_position = default_mouse_position
        except urllib2.URLError, e:
            print "INFRA-ERROR: We couldn't reach hg.mozilla.org: %s" % str(e)
            return 1
        except Exception, e:
            print "ERROR: We were not expecting any more exceptions: %s" % str(e)
            return 1

    current_screen_resolution = queryScreenResolution()
    print "Screen resolution (current): (%(x)s, %(y)s)" % (current_screen_resolution)

    if current_screen_resolution == new_screen_resolution:
        print "No need to change the screen resolution."
    else:
        print "Changing the screen resolution..."
        try:
            changeScreenResolution(new_screen_resolution["x"], new_screen_resolution["y"])
        except Exception, e:
            print "INFRA-ERROR: We have attempted to change the screen resolution but " + \
                  "something went wrong: %s" % str(e)
            return 1
        time.sleep(3)  # just in case
        current_screen_resolution = queryScreenResolution()
        print "Screen resolution (new): (%(x)s, %(y)s)" % current_screen_resolution

    print "Mouse position (current): (%(x)s, %(y)s)" % (queryMousePosition())
    setCursorPos(new_mouse_position["x"], new_mouse_position["y"])
    current_mouse_position = queryMousePosition()
    print "Mouse position (new): (%(x)s, %(y)s)" % (current_mouse_position)

    if current_screen_resolution != new_screen_resolution or current_mouse_position != new_mouse_position:
        print "INFRA-ERROR: The new screen resolution or mouse positions are not what we expected"
        return 1
    else:
        return 0

class POINT(Structure):
    _fields_ = [("x", c_ulong), ("y", c_ulong)]

def queryMousePosition():
    pt = POINT()
    windll.user32.GetCursorPos(byref(pt))
    return { "x": pt.x, "y": pt.y}

def setCursorPos(x, y):
    windll.user32.SetCursorPos(x, y)

def queryScreenResolution():
    return {"x": windll.user32.GetSystemMetrics(0),
            "y": windll.user32.GetSystemMetrics(1)}

def changeScreenResolution(xres = None, yres = None, BitsPerPixel = None):
    import struct

    DM_BITSPERPEL = 0x00040000
    DM_PELSWIDTH = 0x00080000
    DM_PELSHEIGHT = 0x00100000
    CDS_FULLSCREEN = 0x00000004
    SIZEOF_DEVMODE = 148

    DevModeData = struct.calcsize("32BHH") * '\x00'
    DevModeData += struct.pack("H", SIZEOF_DEVMODE)
    DevModeData += struct.calcsize("H") * '\x00'
    dwFields = (xres and DM_PELSWIDTH or 0) | (yres and DM_PELSHEIGHT or 0) | (BitsPerPixel and DM_BITSPERPEL or 0)
    DevModeData += struct.pack("L", dwFields)
    DevModeData += struct.calcsize("l9h32BHL") * '\x00'
    DevModeData += struct.pack("LLL", BitsPerPixel or 0, xres or 0, yres or 0)
    DevModeData += struct.calcsize("8L") * '\x00'

    return windll.user32.ChangeDisplaySettingsA(DevModeData, 0)

if __name__ == '__main__':
    sys.exit(main())
