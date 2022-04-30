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

    response = websocket_request(
        current_browser.remote_agent_port, path=current_browser.debugger_address
    )
    assert response.status == 101
