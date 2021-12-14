import pytest

from http.client import HTTPConnection


def put_required_headers(conn):
    conn.putheader("Connection", "upgrade")
    conn.putheader("Upgrade", "websocket")
    conn.putheader("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==")
    conn.putheader("Sec-WebSocket-Version", "13")


@pytest.mark.parametrize(
    "hostname, port_type, status",
    [
        # Valid hosts
        ("localhost", "remote_agent_port", 101),
        ("localhost", "default_port", 101),
        ("127.0.0.1", "remote_agent_port", 101),
        ("127.0.0.1", "default_port", 101),
        ("[::1]", "remote_agent_port", 101),
        ("[::1]", "default_port", 101),
        ("192.168.8.1", "remote_agent_port", 101),
        ("192.168.8.1", "default_port", 101),
        ("[fdf8:f535:82e4::53]", "remote_agent_port", 101),
        ("[fdf8:f535:82e4::53]", "default_port", 101),
        # Invalid hosts
        ("mozilla.org", "remote_agent_port", 400),
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
@pytest.mark.capabilities({"webSocketUrl": True})
def test_host_header(session, hostname, port_type, status):
    websocket_url = session.capabilities["webSocketUrl"]
    url = websocket_url.replace("ws:", "http:")
    _, _, real_host, path = url.split("/", 3)
    _, remote_agent_port = real_host.split(":")

    def get_host():
        if port_type == "default_port":
            return hostname
        elif port_type == "remote_agent_port":
            return hostname + ":" + remote_agent_port
        elif port_type == "wrong_port":
            wrong_port = str(int(remote_agent_port) + 1)
            return hostname + ":" + wrong_port

    conn = HTTPConnection(real_host)

    conn.putrequest("GET", url, skip_host=True)

    conn.putheader("Host", get_host())
    put_required_headers(conn)
    conn.endheaders()

    response = conn.getresponse()

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
@pytest.mark.capabilities({"webSocketUrl": True})
def test_origin_header(session, origin, status):
    websocket_url = session.capabilities["webSocketUrl"]
    url = websocket_url.replace("ws:", "http:")
    _, _, real_host, path = url.split("/", 3)

    conn = HTTPConnection(real_host)
    conn.putrequest("GET", url)

    if origin is not None:
        conn.putheader("Origin", origin)

    put_required_headers(conn)
    conn.endheaders()

    response = conn.getresponse()

    assert response.status == status
