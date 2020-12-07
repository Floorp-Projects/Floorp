import json

import pytest

from tests.support.http_request import HTTPRequest


def set_context(session, context):
    body = {"context": context}
    session.send_session_command("POST", "moz/context", body)


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
    set_context(session, "chrome")
    assert (
        session.execute_script("""return Services.appinfo.fissionAutostart""") is False
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
    set_context(session, "chrome")
    assert (
        session.execute_script("""return Services.appinfo.fissionAutostart""") is True
    )
