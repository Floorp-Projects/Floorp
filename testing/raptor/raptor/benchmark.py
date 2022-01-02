# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import shutil
import socket

from logger.logger import RaptorLogger
from wptserve import server, handlers

LOG = RaptorLogger(component="raptor-benchmark")
here = os.path.abspath(os.path.dirname(__file__))


class Benchmark(object):
    """utility class for running benchmarks in raptor"""

    def __init__(self, config, test):
        self.config = config
        self.test = test

        # bench_dir is where we will download all mitmproxy required files
        # when running locally it comes from obj_path via mozharness/mach
        if self.config.get("obj_path", None) is not None:
            self.bench_dir = self.config.get("obj_path")
        else:
            # in production it is ../tasks/task_N/build/tests/raptor/raptor/...
            # 'here' is that path, we can start with that
            self.bench_dir = here

        # now add path for benchmark source; locally we put it in a raptor benchmarks
        # folder; in production the files are automatically copied to a different dir
        if self.config.get("run_local", False):
            self.bench_dir = os.path.join(
                self.bench_dir, "testing", "raptor", "benchmarks"
            )
        else:
            self.bench_dir = os.path.join(
                self.bench_dir, "tests", "webkit", "PerformanceTests"
            )

            # Some benchmarks may have been downloaded from a fetch task, make
            # sure they get copied over.
            fetches_dir = os.environ.get("MOZ_FETCHES_DIR")
            if (
                test.get("fetch_task", False)
                and fetches_dir
                and os.path.isdir(fetches_dir)
            ):
                for name in os.listdir(fetches_dir):
                    if test.get("fetch_task").lower() in name.lower():
                        path = os.path.join(fetches_dir, name)
                        if os.path.isdir(path):
                            shutil.copytree(path, os.path.join(self.bench_dir, name))

        LOG.info("bench_dir contains:")
        LOG.info(os.listdir(self.bench_dir))

        # now have the benchmark source ready, go ahead and serve it up!
        self.start_http_server()

    def start_http_server(self):
        self.write_server_headers()

        # pick a free port
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.bind(("", 0))
        self.host = self.config["host"]
        self.port = sock.getsockname()[1]
        sock.close()
        _webserver = "%s:%d" % (self.host, self.port)

        self.httpd = self.setup_webserver(_webserver)
        self.httpd.start()

    def write_server_headers(self):
        # to add specific headers for serving files via wptserve, write out a headers dir file
        # see http://wptserve.readthedocs.io/en/latest/handlers.html#file-handlers
        LOG.info("writing wptserve headers file")
        headers_file = os.path.join(self.bench_dir, "__dir__.headers")
        file = open(headers_file, "w")
        file.write("Access-Control-Allow-Origin: *")
        file.close()
        LOG.info("wrote wpt headers file: %s" % headers_file)

    def setup_webserver(self, webserver):
        LOG.info("starting webserver on %r" % webserver)
        LOG.info("serving benchmarks from here: %s" % self.bench_dir)
        self.host, self.port = webserver.split(":")

        return server.WebTestHttpd(
            host=self.host,
            port=int(self.port),
            doc_root=self.bench_dir,
            routes=[("GET", "*", handlers.file_handler)],
        )

    def stop_serve(self):
        LOG.info("TODO: stop serving benchmark source")
        pass
