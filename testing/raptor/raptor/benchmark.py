# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import shutil
import socket

from mozlog import get_proxy_logger

from wptserve import server, handlers

LOG = get_proxy_logger(component="raptor-benchmark")
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
        if self.config.get('run_local', False):
            self.bench_dir = os.path.join(self.bench_dir, 'testing', 'raptor', 'benchmarks')
        else:
            self.bench_dir = os.path.join(self.bench_dir, 'tests', 'webkit', 'PerformanceTests')

        LOG.info("bench_dir to be used for benchmark source: %s" % self.bench_dir)
        if not os.path.exists(self.bench_dir):
            os.makedirs(self.bench_dir)

        # when running locally we need to get the benchmark source
        if self.config.get('run_local', False):
            self.get_webkit_source()

        LOG.info("bench_dir contains:")
        LOG.info(os.listdir(self.bench_dir))

        # now have the benchmark source ready, go ahead and serve it up!
        self.start_http_server()

    def get_webkit_source(self):
        # in production the build system auto copies webkit source into place;
        # but when run locally we need to do this manually, so that raptor can find it
        if 'speedometer' in self.test['name']:
            # we only want to copy over the source for the benchmark that is about to run
            dest = os.path.join(self.bench_dir, 'Speedometer')
            src = os.path.join(os.environ['MOZ_DEVELOPER_REPO_DIR'], 'third_party',
                               'webkit', 'PerformanceTests', 'Speedometer')
        else:
            # otherwise copy all, but be sure to add each benchmark above instead
            dest = self.bench_dir
            # source for all benchmarks is repo/third_party...
            src = os.path.join(os.environ['MOZ_DEVELOPER_REPO_DIR'], 'third_party',
                               'webkit', 'PerformanceTests')

        if os.path.exists(dest):
            LOG.info("benchmark source already exists at: %s" % dest)
            return

        LOG.info("copying webkit benchmarks from %s to %s" % (src, dest))
        try:
            shutil.copytree(src, dest)
        except Exception:
            LOG.critical("error copying webkit benchmarks from %s to %s" % (src, dest))

    def start_http_server(self):
        self.write_server_headers()

        # pick a free port
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.bind(('', 0))
        self.port = sock.getsockname()[1]
        sock.close()
        _webserver = '127.0.0.1:%d' % self.port

        self.httpd = self.setup_webserver(_webserver)
        self.httpd.start()

    def write_server_headers(self):
        # to add specific headers for serving files via wptserve, write out a headers dir file
        # see http://wptserve.readthedocs.io/en/latest/handlers.html#file-handlers
        LOG.info("writing wptserve headers file")
        headers_file = os.path.join(self.bench_dir, '__dir__.headers')
        file = open(headers_file, 'w')
        file.write("Access-Control-Allow-Origin: *")
        file.close()
        LOG.info("wrote wpt headers file: %s" % headers_file)

    def setup_webserver(self, webserver):
        LOG.info("starting webserver on %r" % webserver)
        LOG.info("serving benchmarks from here: %s" % self.bench_dir)
        self.host, self.port = webserver.split(':')

        return server.WebTestHttpd(port=int(self.port), doc_root=self.bench_dir,
                                   routes=[("GET", "*", handlers.file_handler)])

    def stop_serve(self):
        LOG.info("TODO: stop serving benchmark source")
        pass
