#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Spawns necessary HTTP servers for testing Marionette in child
processes.

"""

import argparse
import multiprocessing
import os
import sys
from collections import defaultdict

import moznetwork

import httpd

__all__ = ["default_doc_root",
           "iter_proc",
           "iter_url",
           "registered_servers",
           "servers",
           "start",
           "where_is"]
here = os.path.abspath(os.path.dirname(__file__))


class BlockingChannel(object):

    def __init__(self, channel):
        self.chan = channel
        self.lock = multiprocessing.Lock()

    def call(self, func, args=()):
        self.send((func, args))
        return self.recv()

    def send(self, *args):
        try:
            self.lock.acquire()
            self.chan.send(args)
        finally:
            self.lock.release()

    def recv(self):
        try:
            self.lock.acquire()
            payload = self.chan.recv()
            if isinstance(payload, tuple) and len(payload) == 1:
                return payload[0]
            return payload
        except KeyboardInterrupt:
            return ("stop", ())
        finally:
            self.lock.release()


class ServerProxy(multiprocessing.Process, BlockingChannel):

    def __init__(self, channel, init_func, *init_args, **init_kwargs):
        multiprocessing.Process.__init__(self)
        BlockingChannel.__init__(self, channel)
        self.init_func = init_func
        self.init_args = init_args
        self.init_kwargs = init_kwargs

    def run(self):
        server = self.init_func(*self.init_args, **self.init_kwargs)
        server.start(block=False)

        try:
            while True:
                # ["func", ("arg", ...)]
                # ["prop", ()]
                sattr, fargs = self.recv()
                attr = getattr(server, sattr)

                # apply fargs to attr if it is a function
                if callable(attr):
                    rv = attr(*fargs)

                # otherwise attr is a property
                else:
                    rv = attr

                self.send(rv)

                if sattr == "stop":
                    return

        except KeyboardInterrupt:
            server.stop()


class ServerProc(BlockingChannel):

    def __init__(self, init_func):
        self._init_func = init_func
        self.proc = None

        parent_chan, self.child_chan = multiprocessing.Pipe()
        BlockingChannel.__init__(self, parent_chan)

    def start(self, doc_root, ssl_config, **kwargs):
        self.proc = ServerProxy(
            self.child_chan, self._init_func, doc_root, ssl_config, **kwargs)
        self.proc.daemon = True
        self.proc.start()

    def get_url(self, url):
        return self.call("get_url", (url,))

    @property
    def doc_root(self):
        return self.call("doc_root", ())

    def stop(self):
        self.call("stop")
        if not self.is_alive:
            return
        self.proc.join()

    def kill(self):
        if not self.is_alive:
            return
        self.proc.terminate()
        self.proc.join(0)

    @property
    def is_alive(self):
        if self.proc is not None:
            return self.proc.is_alive()
        return False


def http_server(doc_root, ssl_config, **kwargs):
    return httpd.FixtureServer(doc_root, url="http://%s:0/" % moznetwork.get_ip())


def https_server(doc_root, ssl_config, **kwargs):
    return httpd.FixtureServer(doc_root,
                               url="https://%s:0/" % moznetwork.get_ip(),
                               ssl_key=ssl_config["key_path"],
                               ssl_cert=ssl_config["cert_path"])


def start_servers(doc_root, ssl_config, **kwargs):
    servers = defaultdict()
    for schema, builder_fn in registered_servers:
        proc = ServerProc(builder_fn)
        proc.start(doc_root, ssl_config, **kwargs)
        servers[schema] = (proc.get_url("/"), proc)
    return servers


def start(doc_root=None, **kwargs):
    """Start all relevant test servers.

    If no `doc_root` is given the default
    testing/marionette/harness/marionette/www directory will be used.

    Additional keyword arguments can be given which will be passed on
    to the individual ``FixtureServer``'s in httpd.py.

    """
    doc_root = doc_root or default_doc_root
    ssl_config = {"cert_path": httpd.default_ssl_cert,
                  "key_path": httpd.default_ssl_key}

    global servers
    servers = start_servers(doc_root, ssl_config, **kwargs)

    return servers


def where_is(uri, on="http"):
    """Returns the full URL, including scheme, hostname, and port, for
    a fixture resource from the server associated with the ``on`` key.
    It will by default look for the resource in the "http" server.

    """
    return servers.get(on)[1].get_url(uri)


def iter_proc(servers):
    for _, (_, proc) in servers.iteritems():
        yield proc


def iter_url(servers):
    for _, (url, _) in servers.iteritems():
        yield url


default_doc_root = os.path.join(os.path.dirname(here), "www")
registered_servers = [("http", http_server),
                      ("https", https_server)]
servers = defaultdict()


def main(args):
    global servers

    parser = argparse.ArgumentParser()
    parser.add_argument("-r", dest="doc_root",
                        help="Path to document root.  Overrides default.")
    args = parser.parse_args()

    servers = start(args.doc_root)
    for url in iter_url(servers):
        print >>sys.stderr, "%s: listening on %s" % (sys.argv[0], url)

    try:
        while any(proc.is_alive for proc in iter_proc(servers)):
            for proc in iter_proc(servers):
                proc.proc.join(1)
    except KeyboardInterrupt:
        for proc in iter_proc(servers):
            proc.kill()


if __name__ == "__main__":
    main(sys.argv[1:])
