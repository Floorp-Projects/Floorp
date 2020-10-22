#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import argparse
import os
import socket
import sys
import six
import webbrowser

from mozlog import commandline, get_proxy_logger
from mozlog.commandline import add_logging_group

here = os.path.abspath(os.path.dirname(__file__))
LOG = get_proxy_logger("profiler")

if six.PY2:
    # Import for Python 2
    from SocketServer import TCPServer
    from SimpleHTTPServer import SimpleHTTPRequestHandler
    from urllib import quote
else:
    # Import for Python 3
    from socketserver import TCPServer
    from http.server import SimpleHTTPRequestHandler
    from urllib.parse import quote


class ProfileServingHTTPRequestHandler(SimpleHTTPRequestHandler):
    """Extends the basic SimpleHTTPRequestHandler (which serves a directory
    of files) to include request headers required by profiler.firefox.com"""

    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "https://profiler.firefox.com")
        SimpleHTTPRequestHandler.end_headers(self)


class ViewGeckoProfile(object):
    """Container class for ViewGeckoProfile"""

    def __init__(self, gecko_profile_data_path):
        self.gecko_profile_data_path = gecko_profile_data_path
        self.gecko_profile_dir = os.path.dirname(gecko_profile_data_path)
        self.profiler_url = "https://profiler.firefox.com/from-url/"
        self.httpd = None
        self.host = "127.0.0.1"
        self.port = None
        self.oldcwd = os.getcwd()

    def setup_http_server(self):
        # pick a free port
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.bind(("", 0))
        self.port = sock.getsockname()[1]
        sock.close()

        # Temporarily change the directory to the profile directory.
        os.chdir(self.gecko_profile_dir)
        self.httpd = TCPServer(
            (self.host, self.port), ProfileServingHTTPRequestHandler
        )

    def handle_single_request(self):
        self.httpd.handle_request()
        # Go back to the old cwd, which some infrastructure may be relying on.
        os.chdir(self.oldcwd)

    def encode_url(self):
        # Encode url i.e.: https://profiler.firefox.com/from-url/http...
        file_url = "http://{}:{}/{}".format(
            self.host, self.port, os.path.basename(self.gecko_profile_data_path)
        )

        self.profiler_url = self.profiler_url + quote(file_url, safe="")
        LOG.info("Temporarily serving the profile from: %s" % file_url)

    def open_profile_in_browser(self):
        # Open the file in the user's preferred browser.
        LOG.info("Opening the profile: %s" % self.profiler_url)
        webbrowser.open_new_tab(self.profiler_url)


def create_parser(mach_interface=False):
    parser = argparse.ArgumentParser()
    add_arg = parser.add_argument

    add_arg(
        "-p",
        "--profile-zip",
        required=True,
        dest="profile_zip",
        help="path to the gecko profiles zip file to open in profiler.firefox.com",
    )

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


def view_gecko_profile(profile_path):
    """
    Open a gecko profile in the user's default browser. This function opens
    up a special URL to profiler.firefox.com and serves up the local profile.
    """
    view_gecko_profile = ViewGeckoProfile(profile_path)

    view_gecko_profile.setup_http_server()
    view_gecko_profile.encode_url()
    view_gecko_profile.open_profile_in_browser()
    view_gecko_profile.handle_single_request()


def start_from_command_line():
    args = parse_args(sys.argv[1:])
    commandline.setup_logging("view-gecko-profile", args, {"tbpl": sys.stdout})

    view_gecko_profile(args.profile_zip)


if __name__ == "__main__":
    start_from_command_line()
