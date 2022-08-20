import pytest

from support.network import websocket_request, get_host


@pytest.mark.parametrize(
    "hostname, port_type, status",
    [
        # Valid hosts
        ("localhost", "server_port", 101),
        ("localhost", "default_port", 101),
        ("127.0.0.1", "server_port", 101),
        ("127.0.0.1", "default_port", 101),
        ("[::1]", "server_port", 101),
        ("[::1]", "default_port", 101),
        ("192.168.8.1", "server_port", 101),
        ("192.168.8.1", "default_port", 101),
        ("[fdf8:f535:82e4::53]", "server_port", 101),
        ("[fdf8:f535:82e4::53]", "default_port", 101),
        # Invalid hosts
        ("mozilla.org", "server_port", 400),
        ("mozilla.org", "wrong_port", 400),
        ("mozilla.org", "default_port", 400),
        ("localhost", "wrong_port", 400),
        ("127.0.0.1", "wrong_port", 400),
        ("[::1]", "wrong_port", 400),
        ("192.168.8.1", "wrong_port", 400),
        ("[fdf8:f535:82e4::53]", "wrong_port", 400),
    ],
    ids=[
        # Valid hosts
        "localhost with same port as RemoteAgent",
        "localhost with default port",
        "127.0.0.1 (loopback) with same port as RemoteAgent",
        "127.0.0.1 (loopback) with default port",
        "[::1] (ipv6 loopback) with same port as RemoteAgent",
        "[::1] (ipv6 loopback) with default port",
        "ipv4 address with same port as RemoteAgent",
        "ipv4 address with default port",
        "ipv6 address with same port as RemoteAgent",
        "ipv6 address with default port",
        # Invalid hosts
        "random hostname with the same port as RemoteAgent",
        "random hostname with a different port than RemoteAgent",
        "random hostname with default port",
        "localhost with a different port than RemoteAgent",
        "127.0.0.1 (loopback) with a different port than RemoteAgent",
        "[::1] (ipv6 loopback) with a different port than RemoteAgent",
        "ipv4 address with a different port than RemoteAgent",
        "ipv6 address with a different port than RemoteAgent",
    ],
)
def test_host_header(browser, hostname, port_type, status):
    # Request a default browser
    current_browser = browser(use_bidi=True)
    server_port = current_browser.remote_agent_port
    test_host = get_host(port_type, hostname, server_port)

    response = websocket_request("127.0.0.1", server_port, host=test_host)
    assert response.status == status


@pytest.mark.parametrize(
    "hostname, port_type, status",
    [
        # Allowed hosts
        ("testhost", "server_port", 101),
        ("testhost", "default_port", 101),
        ("testhost", "wrong_port", 400),
        # IP addresses
        ("192.168.8.1", "server_port", 101),
        ("192.168.8.1", "default_port", 101),
        ("[fdf8:f535:82e4::53]", "server_port", 101),
        ("[fdf8:f535:82e4::53]", "default_port", 101),
        ("127.0.0.1", "server_port", 101),
        ("127.0.0.1", "default_port", 101),
        ("[::1]", "server_port", 101),
        ("[::1]", "default_port", 101),
        # Localhost
        ("localhost", "server_port", 400),
        ("localhost", "default_port", 400),
    ],
    ids=[
        # Allowed hosts
        "allowed host with same port as RemoteAgent",
        "allowed host with default port",
        "allowed host with wrong port",
        # IP addresses
        "ipv4 address with same port as RemoteAgent",
        "ipv4 address with default port",
        "ipv6 address with same port as RemoteAgent",
        "ipv6 address with default port",
        "127.0.0.1 (loopback) with same port as RemoteAgent",
        "127.0.0.1 (loopback) with default port",
        "[::1] (ipv6 loopback) with same port as RemoteAgent",
        "[::1] (ipv6 loopback) with default port",
        # Localhost
        "localhost with same port as RemoteAgent",
        "localhost with default port",
    ],
)
def test_allowed_hosts(browser, hostname, port_type, status):
    # Request a browser with custom allowed hosts.
    current_browser = browser(
        use_bidi=True,
        extra_args=["--remote-allow-hosts", "testhost"],
    )
    server_port = current_browser.remote_agent_port
    test_host = get_host(port_type, hostname, server_port)

    response = websocket_request("127.0.0.1", server_port, host=test_host)
    assert response.status == status


@pytest.mark.parametrize(
    "origin, status",
    [
        (None, 101),
        ("", 400),
        ("sometext", 400),
        ("http://localhost:1234", 400),
    ],
)
def test_origin_header(browser, origin, status):
    # Request a default browser.
    current_browser = browser(use_bidi=True)
    server_port = current_browser.remote_agent_port
    response = websocket_request("127.0.0.1", server_port, origin=origin)
    assert response.status == status


@pytest.mark.parametrize(
    "origin, status",
    [
        (None, 101),
        ("", 400),
        ("sometext", 400),
        ("http://localhost:1234", 101),
        ("https://localhost:1234", 400),
    ],
)
def test_allowed_origins(browser, origin, status):
    # Request a browser with custom allowed origins.
    current_browser = browser(
        use_bidi=True,
        extra_args=["--remote-allow-origins", "http://localhost:1234"],
    )
    server_port = current_browser.remote_agent_port

    # Both `localhost` and `127.0.0.1` have to accept connections.
    for target_host in ["127.0.0.1", "localhost"]:
        print(f"Connecting to the WebSocket via host {target_host}")
        response = websocket_request(target_host, server_port, origin=origin)
        assert response.status == status
