import collections
import contextlib
import threading
import platform
import sys

import pytest
import trustme
from tornado import web, ioloop

from dummyserver.handlers import TestingApp
from dummyserver.server import run_tornado_app
from dummyserver.server import HAS_IPV6


# The Python 3.8+ default loop on Windows breaks Tornado
@pytest.fixture(scope="session", autouse=True)
def configure_windows_event_loop():
    if sys.version_info >= (3, 8) and platform.system() == "Windows":
        import asyncio

        asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())


ServerConfig = collections.namedtuple("ServerConfig", ["host", "port", "ca_certs"])


@contextlib.contextmanager
def run_server_in_thread(scheme, host, tmpdir, ca, server_cert):
    ca_cert_path = str(tmpdir / "ca.pem")
    server_cert_path = str(tmpdir / "server.pem")
    server_key_path = str(tmpdir / "server.key")
    ca.cert_pem.write_to_path(ca_cert_path)
    server_cert.private_key_pem.write_to_path(server_key_path)
    server_cert.cert_chain_pems[0].write_to_path(server_cert_path)
    server_certs = {"keyfile": server_key_path, "certfile": server_cert_path}

    io_loop = ioloop.IOLoop.current()
    app = web.Application([(r".*", TestingApp)])
    server, port = run_tornado_app(app, io_loop, server_certs, scheme, host)
    server_thread = threading.Thread(target=io_loop.start)
    server_thread.start()

    yield ServerConfig(host, port, ca_cert_path)

    io_loop.add_callback(server.stop)
    io_loop.add_callback(io_loop.stop)
    server_thread.join()


@pytest.fixture
def no_san_server(tmp_path_factory):
    tmpdir = tmp_path_factory.mktemp("certs")
    ca = trustme.CA()
    # only common name, no subject alternative names
    server_cert = ca.issue_cert(common_name=u"localhost")

    with run_server_in_thread("https", "localhost", tmpdir, ca, server_cert) as cfg:
        yield cfg


@pytest.fixture
def ip_san_server(tmp_path_factory):
    tmpdir = tmp_path_factory.mktemp("certs")
    ca = trustme.CA()
    # IP address in Subject Alternative Name
    server_cert = ca.issue_cert(u"127.0.0.1")

    with run_server_in_thread("https", "127.0.0.1", tmpdir, ca, server_cert) as cfg:
        yield cfg


@pytest.fixture
def ipv6_addr_server(tmp_path_factory):
    if not HAS_IPV6:
        pytest.skip("Only runs on IPv6 systems")

    tmpdir = tmp_path_factory.mktemp("certs")
    ca = trustme.CA()
    # IP address in Common Name
    server_cert = ca.issue_cert(common_name=u"::1")

    with run_server_in_thread("https", "::1", tmpdir, ca, server_cert) as cfg:
        yield cfg


@pytest.fixture
def ipv6_san_server(tmp_path_factory):
    if not HAS_IPV6:
        pytest.skip("Only runs on IPv6 systems")

    tmpdir = tmp_path_factory.mktemp("certs")
    ca = trustme.CA()
    # IP address in Subject Alternative Name
    server_cert = ca.issue_cert(u"::1")

    with run_server_in_thread("https", "::1", tmpdir, ca, server_cert) as cfg:
        yield cfg
