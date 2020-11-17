import json

from tests.support.asserts import assert_success
from tests.support.http_request import HTTPRequest


def test_debugger_address_not_set(new_session, configuration):
    capabilities = dict(configuration["capabilities"])

    response, _ = new_session({"capabilities": {"alwaysMatch": capabilities}})
    result = assert_success(response)

    debugger_address = result["capabilities"].get("moz:debuggerAddress")
    assert debugger_address is None


def test_debugger_address_false(new_session, configuration):
    capabilities = dict(configuration["capabilities"])
    capabilities["moz:debuggerAddress"] = False

    response, _ = new_session({"capabilities": {"alwaysMatch": capabilities}})
    result = assert_success(response)

    debugger_address = result["capabilities"].get("moz:debuggerAddress")
    assert debugger_address is None


def test_debugger_address_true(new_session, configuration):
    capabilities = dict(configuration["capabilities"])
    capabilities["moz:debuggerAddress"] = True

    response, _ = new_session({"capabilities": {"alwaysMatch": capabilities}})
    result = assert_success(response)

    debugger_address = result["capabilities"].get("moz:debuggerAddress")
    assert debugger_address is not None

    host, port = debugger_address.split(":")
    assert host == "localhost"
    assert port.isnumeric()

    # Fetch the browser version via the debugger address
    http = HTTPRequest(host, int(port))
    with http.get("/json/version") as response:
        data = json.loads(response.read())
        assert result["capabilities"]["browserVersion"] in data["Browser"]
