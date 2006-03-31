#!/usr/bin/env python
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
# The Original Code is python code to drive jssh-enabled apps.
#
# The Initial Developer of the Original Code is
# davel@mozilla.com.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Grig Gheorghiu <grig@gheorghiu.net>
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
import re
import sys
import telnetlib

class JsshDriver:

    COMMAND_PROMPT = "\n> "

    def __init__(self, host=None, port=9997, telnetklass=telnetlib.Telnet):
        """Ctor
        	Ctor
        """
        self.host = host
        self.port = port
        self.tn = telnetklass(host,port)
        self.init()

    def open(self,host,port=9997):
        self.tn.close()
        self.host = host
        self.port = port
        self.tn.open(host,port)
        self.init()

    def init(self):
        if not self.tn.get_socket():
            return

        self.tn.read_until(JsshDriver.COMMAND_PROMPT)
        self.send_command("setProtocol('synchronous')")
	
    def send_command(self,command):
        self.tn.write(command + "\n")
        return self.tn.read_until(JsshDriver.COMMAND_PROMPT)

    def send_quit(self):
        self.tn.write("quit()\n")
        return self.tn.read_all()

class JsshTester:

    def __init__(self, host, port=9997, telnetklass=telnetlib.Telnet):
        self.browser = JsshDriver(host,port,telnetklass)

    def __del__(self):
        self.browser.send_quit()

    def get_innerHTML_from_URL(self,url):
        self.browser.send_command ('var browser = getWindows()[0].getBrowser()')

        if url:
            self.browser.send_command ('browser.loadURI("' + url + '")')

        self.browser.send_command ('var document = browser.contentDocument')
        self.browser.send_command ('var window = browser.contentWindow')
        jssh_response = self.browser.send_command ('print(document.documentElement.innerHTML)')

        m = re.compile(r"\[(?P<len>\d+)](?P<rest>.*)", re.DOTALL).search(jssh_response)

        return m.group('rest')[0:int(m.group('len'))]
