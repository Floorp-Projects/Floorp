#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

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
from mozrunner import runners

from wptserve import server, handlers

here = os.path.abspath(os.path.dirname(__file__))


class ViewGeckoProfile(object):
    """Container class for ViewGeckoProfile"""

    def __init__(self, browser_binary, gecko_profile_zip):
        self.log = get_default_logger(component='view-gecko-profile')
        self.browser_bin = browser_binary
        self.gecko_profile_zip = gecko_profile_zip
        self.gecko_profile_dir = os.path.dirname(gecko_profile_zip)
        self.perfhtmlio_url = "https://perf-html.io/from-url/"
        self.httpd = None
        self.host = None
        self.port = None

        # Create the runner
        self.output_handler = OutputHandler()
        process_args = {
            'processOutputLine': [self.output_handler],
        }
        runner_cls = runners['firefox']
        self.runner = runner_cls(
            browser_binary, profile=None, process_args=process_args)

    def start_http_server(self):
        self.write_server_headers()

        # pick a free port
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.bind(('', 0))
        port = sock.getsockname()[1]
        sock.close()
        _webserver = '127.0.0.1:%d' % port

        self.httpd = self.setup_webserver(_webserver)
        self.httpd.start()

    def write_server_headers(self):
        # to add specific headers for serving files via wptserve, write out a headers dir file
        # see http://wptserve.readthedocs.io/en/latest/handlers.html#file-handlers
        self.log.info("writing wptserve headers file")
        headers_file = os.path.join(self.gecko_profile_dir, '__dir__.headers')
        file = open(headers_file, 'w')
        file.write("Access-Control-Allow-Origin: *")
        file.close()
        self.log.info("wrote wpt headers file: %s" % headers_file)

    def setup_webserver(self, webserver):
        self.log.info("starting webserver on %r" % webserver)
        self.host, self.port = webserver.split(':')

        return server.WebTestHttpd(port=int(self.port), doc_root=self.gecko_profile_dir,
                            routes=[("GET", "*", handlers.file_handler)])

    def encode_url(self):
        # Encode url i.e.: https://perf-html.io/from-url/http... (the profile_zip served locally)
        url = 'http://' + self.host + ':' + self.port + '/' + os.path.basename(self.gecko_profile_zip)
        self.log.info("raw url is:")
        self.log.info(url)
        encoded_url = urllib.quote(url, safe='')
        self.log.info('encoded url is:')
        self.log.info(encoded_url)
        self.perfhtmlio_url = self.perfhtmlio_url + encoded_url
        self.log.info('full url is:')
        self.log.info(self.perfhtmlio_url)

    def start_browser_perfhtml(self):
        self.log.info("Starting browser and opening the gecko profile zip in perf-html.io...")

        self.runner.cmdargs.append(self.perfhtmlio_url)
        self.runner.start()

        first_time = int(time.time()) * 1000
        proc = self.runner.process_handler
        self.output_handler.proc = proc

        try:
            self.runner.wait(timeout=None)
        except Exception:
            self.log.info("Failed to start browser")


class OutputHandler(object):
    def __init__(self):
        self.proc = None
        self.kill_thread = Thread(target=self.wait_for_quit)
        self.kill_thread.daemon = True
        self.log = get_default_logger(component='view_gecko_profile')

    def __call__(self, line):
        if not line.strip():
            return
        line = line.decode('utf-8', errors='replace')

        try:
            data = json.loads(line)
        except ValueError:
            self.process_output(line)
            return

        if isinstance(data, dict) and 'action' in data:
            self.log.log_raw(data)
        else:
            self.process_output(json.dumps(data))

    def process_output(self, line):
        self.log.process_output(self.proc.pid, line)

    def wait_for_quit(self, timeout=5):
        """Wait timeout seconds for the process to exit. If it hasn't
        exited by then, kill it.
        """
        time.sleep(timeout)
        if self.proc.poll() is None:
            self.proc.kill()


def create_parser(mach_interface=False):
    parser = argparse.ArgumentParser()
    add_arg = parser.add_argument

    add_arg('-b', '--binary', required=True, dest='binary',
            help="path to browser executable")
    add_arg('-p', '--profile-zip', required=True, dest='profile_zip',
            help="path to the gecko profiles zip file to open in perf-html.io")

    add_logging_group(parser)
    return parser


def verify_options(parser, args):
    ctx = vars(args)

    if not os.path.isfile(args.binary):
        parser.error("{binary} does not exist!".format(**ctx))

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

    view_gecko_profile = ViewGeckoProfile(args.binary, args.profile_zip)

    view_gecko_profile.start_http_server()
    view_gecko_profile.encode_url()
    view_gecko_profile.start_browser_perfhtml()


if __name__ == "__main__":
    main()
