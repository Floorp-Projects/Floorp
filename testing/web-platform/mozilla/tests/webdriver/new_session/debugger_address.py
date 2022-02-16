import json

import pytest

from http.client import HTTPConnection

from tests.support.http_request import HTTPRequest
from . import using_context


def test_debugger_address_not_set(session):
    debugger_address = session.capabilities.get("moz:debuggerAddress")
    assert debugger_address is None


@pytest.mark.capabilities({"moz:debuggerAddress": False})
def test_debugger_address_false(session):
    debugger_address = session.capabilities.get("moz:debuggerAddress")
    assert debugger_address is None


@pytest.mark.capabilities({"moz:debuggerAddress": True})
def test_debugger_address_true_fission_disabled(session):
    debugger_address = session.capabilities.get("moz:debuggerAddress")
    assert debugger_address is not None

    host, port = debugger_address.split(":")
    assert host == "localhost"
    assert port.isnumeric()

    # Fetch the browser version via the debugger address
    http = HTTPRequest(host, int(port))
    with http.get("/json/version") as response:
        data = json.loads(response.read())
        assert session.capabilities["browserVersion"] in data["Browser"]

    # Force disabling Fission until Remote Agent is compatible
    with using_context(session, "chrome"):
        assert (
            session.execute_script("""return Services.appinfo.fissionAutostart""")
            is False
        )


@pytest.mark.capabilities(
    {
        "moz:debuggerAddress": True,
        "moz:firefoxOptions": {
            "prefs": {
                "fission.autostart": True,
            }
        },
    }
)
def test_debugger_address_true_fission_override(session):
    debugger_address = session.capabilities.get("moz:debuggerAddress")
    assert debugger_address is not None

    host, port = debugger_address.split(":")
    assert host == "localhost"
    assert port.isnumeric()

    # Fetch the browser version via the debugger address
    http = HTTPRequest(host, int(port))
    with http.get("/json/version") as response:
        data = json.loads(response.read())
        assert session.capabilities["browserVersion"] in data["Browser"]

    # Allow Fission to be enabled when setting the preference
    with using_context(session, "chrome"):
        assert (
            session.execute_script("""return Services.appinfo.fissionAutostart""")
            is True
        )


@pytest.mark.parametrize("origin", [None, "", "sometext", "http://localhost:1234"])
@pytest.mark.capabilities(
    {
        "moz:debuggerAddress": True,
        "moz:firefoxOptions": {
            "prefs": {
                "remote.active-protocols": 2,
            }
        },
    }
)
def test_origin_header_allowed_when_bidi_disabled(session, origin):
    debugger_address = session.capabilities.get("moz:debuggerAddress")
    assert debugger_address is not None

    url = f"http://{debugger_address}/json/version"

    conn = HTTPConnection(debugger_address)
    conn.putrequest("GET", url)

    if origin is not None:
        conn.putheader("Origin", origin)

    conn.putheader("Connection", "upgrade")
    conn.putheader("Upgrade", "websocket")
    conn.putheader("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==")
    conn.putheader("Sec-WebSocket-Version", "13")
    conn.endheaders()

    response = conn.getresponse()

    assert response.status == 200
