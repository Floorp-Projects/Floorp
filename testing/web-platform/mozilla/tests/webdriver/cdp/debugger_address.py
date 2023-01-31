# META: timeout=long

import json

import pytest
from support.context import using_context
from tests.support.http_request import HTTPRequest


def test_debugger_address_not_set(session):
    debugger_address = session.capabilities.get("moz:debuggerAddress")
    assert debugger_address is None


@pytest.mark.capabilities({"moz:debuggerAddress": False})
def test_debugger_address_false(session):
    debugger_address = session.capabilities.get("moz:debuggerAddress")
    assert debugger_address is None


@pytest.mark.capabilities({"moz:debuggerAddress": True})
@pytest.mark.parametrize("fission_enabled", [True, False], ids=["enabled", "disabled"])
def test_debugger_address_true_with_fission(session, fission_enabled):
    debugger_address = session.capabilities.get("moz:debuggerAddress")
    assert debugger_address is not None

    host, port = debugger_address.split(":")
    assert host == "127.0.0.1"
    assert port.isnumeric()

    # Fetch the browser version via the debugger address, `localhost` has
    # to work as well.
    for target_host in [host, "localhost"]:
        print(f"Connecting to WebSocket via host {target_host}")
        http = HTTPRequest(target_host, int(port))
        with http.get("/json/version") as response:
            data = json.loads(response.read())
            assert session.capabilities["browserVersion"] in data["Browser"]

    # Ensure Fission is not disabled (bug 1813981)
    with using_context(session, "chrome"):
        assert (
            session.execute_script("""return Services.appinfo.fissionAutostart""")
            is fission_enabled
        )
