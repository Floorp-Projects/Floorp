import os

from support.network import websocket_request


def test_devtools_active_port_file(browser):
    current_browser = browser(use_cdp=True)

    assert current_browser.remote_agent_port != 0
    assert current_browser.debugger_address.startswith("/devtools/browser/")

    port_file = os.path.join(current_browser.profile.profile, "DevToolsActivePort")
    assert os.path.exists(port_file)

    current_browser.quit(clean_profile=False)
    assert not os.path.exists(port_file)


def test_connect(browser):
    current_browser = browser(use_cdp=True)

    # Both `localhost` and `127.0.0.1` have to accept connections.
    for target_host in ["127.0.0.1", "localhost"]:
        print(f"Connecting to the WebSocket via host {target_host}")
        response = websocket_request(
            target_host,
            current_browser.remote_agent_port,
            path=current_browser.debugger_address,
        )
        assert response.status == 101
