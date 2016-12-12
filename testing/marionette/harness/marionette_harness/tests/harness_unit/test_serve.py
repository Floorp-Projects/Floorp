# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import types

import pytest

from marionette_harness.runner import serve
from marionette_harness.runner.serve import iter_proc, iter_url


def teardown_function(func):
    for server in [server for server in iter_proc(serve.servers) if server.is_alive]:
        server.stop()
        server.kill()


def test_registered_servers():
    # [(name, factory), ...]
    assert serve.registered_servers[0][0] == "http"
    assert serve.registered_servers[1][0] == "https"


def test_globals():
    assert serve.default_doc_root is not None
    assert serve.registered_servers is not None
    assert serve.servers is not None


def test_start():
    serve.start()
    assert len(serve.servers) == 2
    assert "http" in serve.servers
    assert "https" in serve.servers
    for url in iter_url(serve.servers):
        assert isinstance(url, types.StringTypes)


def test_start_with_custom_root(tmpdir_factory):
    tdir = tmpdir_factory.mktemp("foo")
    serve.start(str(tdir))
    for server in iter_proc(serve.servers):
        assert server.doc_root == tdir


def test_iter_proc():
    serve.start()
    for server in iter_proc(serve.servers):
        server.stop()


def test_iter_url():
    serve.start()
    for url in iter_url(serve.servers):
        assert isinstance(url, types.StringTypes)


def test_where_is():
    serve.start()
    assert serve.where_is("/") == serve.servers["http"][1].get_url("/")
    assert serve.where_is("/", on="https") == serve.servers["https"][1].get_url("/")


if __name__ == "__main__":
    import sys
    sys.exit(pytest.main(["-s", "--verbose", __file__]))
