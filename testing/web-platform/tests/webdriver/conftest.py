import json
import os
import urlparse

import pytest
import webdriver

from util import cleanup
from util.http_request import HTTPRequest

default_host = "http://127.0.0.1"
default_port = "4444"


@pytest.fixture(scope="session")
def _session(request):
    host = os.environ.get("WD_HOST", default_host)
    port = int(os.environ.get("WD_PORT", default_port))
    capabilities = json.loads(os.environ.get("WD_CAPABILITIES", "{}"))

    session = webdriver.Session(host, port, desired_capabilities=capabilities)

    def destroy():
        if session.session_id is not None:
            session.end()

    request.addfinalizer(destroy)

    return session


@pytest.fixture(scope="function")
def session(_session, request):
    # finalisers are popped off a stack,
    # making their ordering reverse
    request.addfinalizer(lambda: cleanup.switch_to_top_level_browsing_context(_session))
    request.addfinalizer(lambda: cleanup.restore_windows(_session))
    request.addfinalizer(lambda: cleanup.dismiss_user_prompts(_session))
    request.addfinalizer(lambda: cleanup.ensure_valid_window(_session))

    return _session


@pytest.fixture(scope="function")
def http(session):
    return HTTPRequest(session.transport.host, session.transport.port)


@pytest.fixture
def server_config():
    return json.loads(os.environ.get("WD_SERVER_CONFIG"))


@pytest.fixture
def url(server_config):
    def inner(path, query="", fragment=""):
        rv = urlparse.urlunsplit(("http",
                                  "%s:%s" % (server_config["host"],
                                             server_config["ports"]["http"][0]),
                                  path,
                                  query,
                                  fragment))
        return rv
    return inner
