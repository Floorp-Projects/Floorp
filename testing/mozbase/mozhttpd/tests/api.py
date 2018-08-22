#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import collections
import json
import os

import pytest
from six.moves.urllib.error import HTTPError
from six.moves.urllib.request import (
    HTTPHandler,
    ProxyHandler,
    Request,
    build_opener,
    install_opener,
    urlopen,
)

import mozunit
import mozhttpd


def httpd_url(httpd, path, querystr=None):
    """Return the URL to a started MozHttpd server for the given info."""

    url = "http://127.0.0.1:{port}{path}".format(
        port=httpd.httpd.server_port,
        path=path,
    )

    if querystr is not None:
        url = "{url}?{querystr}".format(
            url=url,
            querystr=querystr,
        )

    return url


@pytest.fixture(name="num_requests")
def fixture_num_requests():
    """Return a defaultdict to count requests to HTTP handlers."""
    return collections.defaultdict(int)


@pytest.fixture(name="try_get")
def fixture_try_get(num_requests):
    """Return a function to try GET requests to the server."""

    def try_get(httpd, querystr):
        """Try GET requests to the server."""

        num_requests["get_handler"] = 0

        f = urlopen(httpd_url(httpd, "/api/resource/1", querystr))

        assert f.getcode() == 200
        assert json.loads(f.read()) == {"called": 1, "id": "1", "query": querystr}
        assert num_requests["get_handler"] == 1

    return try_get


@pytest.fixture(name="try_post")
def fixture_try_post(num_requests):
    """Return a function to try POST calls to the server."""

    def try_post(httpd, querystr):
        """Try POST calls to the server."""

        num_requests["post_handler"] = 0

        postdata = {"hamburgers": "1234"}

        f = urlopen(
            httpd_url(httpd, "/api/resource/", querystr),
            data=json.dumps(postdata),
        )

        assert f.getcode() == 201
        assert json.loads(f.read()) == {
            "called": 1,
            "data": postdata,
            "query": querystr,
        }
        assert num_requests["post_handler"] == 1

    return try_post


@pytest.fixture(name="try_del")
def fixture_try_del(num_requests):
    """Return a function to try DEL calls to the server."""

    def try_del(httpd, querystr):
        """Try DEL calls to the server."""

        num_requests["del_handler"] = 0

        opener = build_opener(HTTPHandler)
        request = Request(httpd_url(httpd, "/api/resource/1", querystr))
        request.get_method = lambda: "DEL"
        f = opener.open(request)

        assert f.getcode() == 200
        assert json.loads(f.read()) == {"called": 1, "id": "1", "query": querystr}
        assert num_requests["del_handler"] == 1

    return try_del


@pytest.fixture(name="httpd_no_urlhandlers")
def fixture_httpd_no_urlhandlers():
    """Yields a started MozHttpd server with no URL handlers."""
    httpd = mozhttpd.MozHttpd(port=0)
    httpd.start(block=False)
    yield httpd
    httpd.stop()


@pytest.fixture(name="httpd_with_docroot")
def fixture_httpd_with_docroot(num_requests):
    """Yields a started MozHttpd server with docroot set."""

    @mozhttpd.handlers.json_response
    def get_handler(request, objid):
        """Handler for HTTP GET requests."""

        num_requests["get_handler"] += 1

        return (
            200,
            {
                "called": num_requests["get_handler"],
                "id": objid,
                "query": request.query,
            },
        )

    httpd = mozhttpd.MozHttpd(
        port=0,
        docroot=os.path.dirname(os.path.abspath(__file__)),
        urlhandlers=[
            {
                "method": "GET",
                "path": "/api/resource/([^/]+)/?",
                "function": get_handler,
            }
        ],
    )

    httpd.start(block=False)
    yield httpd
    httpd.stop()


@pytest.fixture(name="httpd")
def fixture_httpd(num_requests):
    """Yield a started MozHttpd server."""

    @mozhttpd.handlers.json_response
    def get_handler(request, objid):
        """Handler for HTTP GET requests."""

        num_requests["get_handler"] += 1

        return (
            200,
            {
                "called": num_requests["get_handler"],
                "id": objid,
                "query": request.query,
            },
        )

    @mozhttpd.handlers.json_response
    def post_handler(request):
        """Handler for HTTP POST requests."""

        num_requests["post_handler"] += 1

        return (
            201,
            {
                "called": num_requests["post_handler"],
                "data": json.loads(request.body),
                "query": request.query,
            },
        )

    @mozhttpd.handlers.json_response
    def del_handler(request, objid):
        """Handler for HTTP DEL requests."""

        num_requests["del_handler"] += 1

        return (
            200,
            {
                "called": num_requests["del_handler"],
                "id": objid,
                "query": request.query,
            },
        )

    httpd = mozhttpd.MozHttpd(
        port=0,
        urlhandlers=[
            {
                "method": "GET",
                "path": "/api/resource/([^/]+)/?",
                "function": get_handler,
            },
            {
                "method": "POST",
                "path": "/api/resource/?",
                "function": post_handler,
            },
            {
                "method": "DEL",
                "path": "/api/resource/([^/]+)/?",
                "function": del_handler,
            },
        ],
    )

    httpd.start(block=False)
    yield httpd
    httpd.stop()


def test_api(httpd, try_get, try_post, try_del):
    # GET requests
    try_get(httpd, "")
    try_get(httpd, "?foo=bar")

    # POST requests
    try_post(httpd, "")
    try_post(httpd, "?foo=bar")

    # DEL requests
    try_del(httpd, "")
    try_del(httpd, "?foo=bar")

    # GET: By default we don't serve any files if we just define an API
    with pytest.raises(HTTPError) as exc_info:
        urlopen(httpd_url(httpd, "/"))

    assert exc_info.value.code == 404


def test_nonexistent_resources(httpd_no_urlhandlers):
    # GET: Return 404 for non-existent endpoint
    with pytest.raises(HTTPError) as excinfo:
        urlopen(httpd_url(httpd_no_urlhandlers, "/api/resource/"))
    assert excinfo.value.code == 404

    # POST: POST should also return 404
    with pytest.raises(HTTPError) as excinfo:
        urlopen(httpd_url(httpd_no_urlhandlers, "/api/resource/"), data=json.dumps({}))
    assert excinfo.value.code == 404

    # DEL: DEL should also return 404
    opener = build_opener(HTTPHandler)
    request = Request(httpd_url(httpd_no_urlhandlers, "/api/resource/"))
    request.get_method = lambda: "DEL"

    with pytest.raises(HTTPError) as excinfo:
        opener.open(request)
    assert excinfo.value.code == 404


def test_api_with_docroot(httpd_with_docroot, try_get):
    f = urlopen(httpd_url(httpd_with_docroot, "/"))
    assert f.getcode() == 200
    assert "Directory listing for" in f.read()

    # Make sure API methods still work
    try_get(httpd_with_docroot, "")
    try_get(httpd_with_docroot, "?foo=bar")


def index_contents(host):
    """Return the expected index contents for the given host."""
    return "{host} index".format(host=host)


@pytest.fixture(name="hosts")
def fixture_hosts():
    """Returns a tuple of hosts."""
    return ("mozilla.com", "mozilla.org")


@pytest.fixture(name="docroot")
def fixture_docroot(tmpdir):
    """Returns a path object to a temporary docroot directory."""
    docroot = tmpdir.mkdir("docroot")
    index_file = docroot.join("index.html")
    index_file.write(index_contents("*"))

    yield docroot

    docroot.remove()


@pytest.fixture(name="httpd_with_proxy_handler")
def fixture_httpd_with_proxy_handler(docroot):
    """Yields a started MozHttpd server for the proxy test."""

    httpd = mozhttpd.MozHttpd(port=0, docroot=str(docroot))
    httpd.start(block=False)

    port = httpd.httpd.server_port
    proxy_support = ProxyHandler(
        {
            "http": "http://127.0.0.1:{port:d}".format(port=port),
        }
    )
    install_opener(build_opener(proxy_support))

    yield httpd

    httpd.stop()

    # Reset proxy opener in case it changed
    install_opener(None)


def test_proxy(httpd_with_proxy_handler, hosts):
    for host in hosts:
        f = urlopen("http://{host}/".format(host=host))
        assert f.getcode() == 200
        assert f.read() == index_contents("*")


@pytest.fixture(name="httpd_with_proxy_host_dirs")
def fixture_httpd_with_proxy_host_dirs(docroot, hosts):
    for host in hosts:
        index_file = docroot.mkdir(host).join("index.html")
        index_file.write(index_contents(host))

    httpd = mozhttpd.MozHttpd(port=0, docroot=str(docroot), proxy_host_dirs=True)

    httpd.start(block=False)

    port = httpd.httpd.server_port
    proxy_support = ProxyHandler(
        {"http": "http://127.0.0.1:{port:d}".format(port=port)}
    )
    install_opener(build_opener(proxy_support))

    yield httpd

    httpd.stop()

    # Reset proxy opener in case it changed
    install_opener(None)


def test_proxy_separate_directories(httpd_with_proxy_host_dirs, hosts):
    for host in hosts:
        f = urlopen("http://{host}/".format(host=host))
        assert f.getcode() == 200
        assert f.read() == index_contents(host)

    unproxied_host = "notmozilla.org"

    with pytest.raises(HTTPError) as excinfo:
        urlopen("http://{host}/".format(host=unproxied_host))

    assert excinfo.value.code == 404


if __name__ == "__main__":
    mozunit.main()
