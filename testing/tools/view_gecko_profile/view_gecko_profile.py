#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import sys


import argparse
import json
import os
import socket
import sys
import time
import urllib
from threading import Thread

from mozlog import commandline, get_default_logger
from mozlog.commandline import add_logging_group

import SocketServer
import SimpleHTTPServer

here = os.path.abspath(os.path.dirname(__file__))

class ProfileServingHTTPRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    """Extends the basic SimpleHTTPRequestHandler (which serves a directory
    of files) to include request headers required by profiler.firefox.com"""
    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "https://profiler.firefox.com")
        SimpleHTTPServer.SimpleHTTPRequestHandler.end_headers(self)


class ViewGeckoProfile(object):
    """Container class for ViewGeckoProfile"""

    def __init__(self, gecko_profile_data_path):
        self.log = get_default_logger(component='view-gecko-profile')
        self.gecko_profile_data_path = gecko_profile_data_path
        self.gecko_profile_dir = os.path.dirname(gecko_profile_data_path)
        self.profiler_url = "https://profiler.firefox.com/from-url/"
        self.httpd = None
        self.host = '127.0.0.1'
        self.port = None

    def setup_http_server(self):
        # pick a free port
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.bind(('', 0))
        self.port = sock.getsockname()[1]
        sock.close()

        os.chdir(self.gecko_profile_dir)
        self.httpd = SocketServer.TCPServer((self.host, self.port), ProfileServingHTTPRequestHandler)
        self.log.info("File server started at: %s:%s" % (self.host, self.port))

    def handle_single_request(self):
        self.httpd.handle_request()

    def encode_url(self):
        # Encode url i.e.: https://profiler.firefox.com/from-url/http... (the profile_zip served locally)
        url = "http://{}:{}/{}".format(self.host, self.port,
                                       os.path.basename(self.path_to_gecko_profile_data))
        self.log.info("raw url is:")
        self.log.info(url)
        encoded_url = urllib.quote(url, safe='')
        self.log.info('encoded url is:')
        self.log.info(encoded_url)
        self.profiler_url = self.profiler_url + encoded_url
        self.log.info('full url is:')
        self.log.info(self.profiler_url)

    def open_profile_in_browser(self):
        # Open the file in the user's preferred browser.
        self.log.info("Opening the profile data in profiler.firefox.com...")
        import webbrowser
        webbrowser.open_new_tab(self.profiler_url)


def create_parser(mach_interface=False):
    parser = argparse.ArgumentParser()
    add_arg = parser.add_argument

    add_arg('-p', '--profile-zip', required=True, dest='profile_zip',
            help="path to the gecko profiles zip file to open in profiler.firefox.com")

    add_logging_group(parser)
    return parser


def verify_options(parser, args):
    ctx = vars(args)

    if not os.path.isfile(args.profile_zip):
        parser.error("{profile_zip} does not exist!".format(**ctx))


def parse_args(argv=None):
    parser = create_parser()
    args = parser.parse_args(argv)
    verify_options(parser, args)
    return args


def main(args=sys.argv[1:]):
    args = parse_args()
    commandline.setup_logging('view-gecko-profile', args, {'tbpl': sys.stdout})
    LOG = get_default_logger(component='view-gecko-profile')

    view_gecko_profile = ViewGeckoProfile(args.profile_zip)

    view_gecko_profile.setup_http_server()
    view_gecko_profile.encode_url()
    view_gecko_profile.open_profile_in_browser()
    view_gecko_profile.handle_single_request()

if __name__ == "__main__":
    main()
