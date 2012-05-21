#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
