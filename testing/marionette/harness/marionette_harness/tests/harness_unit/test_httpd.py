# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import json
import os
import types
import urllib2

import mozunit
import pytest

from wptserve.handlers import json_handler

from marionette_harness.runner import httpd

here = os.path.abspath(os.path.dirname(__file__))
parent = os.path.dirname(here)
default_doc_root = os.path.join(os.path.dirname(parent), "www")


@pytest.yield_fixture
def server():
    server = httpd.FixtureServer(default_doc_root)
    yield server
    server.stop()


def test_ctor():
    with pytest.raises(ValueError):
        httpd.FixtureServer("foo")
    httpd.FixtureServer(default_doc_root)


def test_start_stop(server):
    server.start()
    server.stop()


def test_get_url(server):
    server.start()
    url = server.get_url("/")
    assert isinstance(url, types.StringTypes)
    assert "http://" in url

    server.stop()
    with pytest.raises(httpd.NotAliveError):
        server.get_url("/")


def test_doc_root(server):
    server.start()
    assert isinstance(server.doc_root, types.StringTypes)
    server.stop()
    assert isinstance(server.doc_root, types.StringTypes)


def test_router(server):
    assert server.router is not None


def test_routes(server):
    assert server.routes is not None


def test_is_alive(server):
    assert server.is_alive == False
    server.start()
    assert server.is_alive == True


def test_handler(server):
    counter = 0

    @json_handler
    def handler(request, response):
        return {"count": counter}

    route = ("GET", "/httpd/test_handler", handler)
    server.router.register(*route)
    server.start()

    url = server.get_url("/httpd/test_handler")
    body = urllib2.urlopen(url).read()
    res = json.loads(body)
    assert res["count"] == counter


if __name__ == "__main__":
    mozunit.main('-p', 'no:terminalreporter', '--log-tbpl=-')
