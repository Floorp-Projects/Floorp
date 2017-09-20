# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This is a little HTTP server that runs "screencap" on the connected
# Android device via "adb shell". This allows the Android device
# itself to trigger "screencap" via HTTP.
#
# We are running emulators without "-gpu off" and with that we are not
# able to take screenshots via UIAutomator. Other ways of taking
# screenshots from the Android device itself do not capture the full
# screen as the user sees it.

from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer

from os import curdir, sep
import subprocess

class ScreenshotHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-type', 'text/plain')
        self.end_headers()

    	print " * Taking screenshot"
        
        command = "adb shell screencap -p /data/data/org.mozilla.focus.debug/files/temp_screen.png"
        process = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE)
        process.wait()

        self.wfile.write("screenshot, exit=" + str(process.returncode))

        print " * Done ( exit = ", process.returncode, ")"

if __name__ == "__main__":
    httpd = HTTPServer(('0.0.0.0', 9771), ScreenshotHandler)
    print 'Starting httpd...'
    httpd.serve_forever()

