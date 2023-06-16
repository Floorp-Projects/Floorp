import pytest
from support.network import get_host, http_request


@pytest.mark.parametrize(
    "hostname, port_type, status",
    [
        # Valid hosts
        ("localhost", "server_port", 200),
        ("127.0.0.1", "server_port", 200),
        ("[::1]", "server_port", 200),
        ("192.168.8.1", "server_port", 200),
        ("[fdf8:f535:82e4::53]", "server_port", 200),
        # Invalid hosts
        ("localhost", "default_port", 500),
        ("127.0.0.1", "default_port", 500),
        ("[::1]", "default_port", 500),
        ("192.168.8.1", "default_port", 500),
        ("[fdf8:f535:82e4::53]", "default_port", 500),
        ("example.org", "server_port", 500),
        ("example.org", "wrong_port", 500),
        ("example.org", "default_port", 500),
        ("localhost", "wrong_port", 500),
        ("127.0.0.1", "wrong_port", 500),
        ("[::1]", "wrong_port", 500),
        ("192.168.8.1", "wrong_port", 500),
        ("[fdf8:f535:82e4::53]", "wrong_port", 500),
    ],
    ids=[
        # Valid hosts
        "localhost with same port as server",
        "127.0.0.1 (loopback) with same port as server",
        "[::1] (ipv6 loopback) with same port as server",
        "ipv4 address with same port as server",
        "ipv6 address with same port as server",
        # Invalid hosts
        "localhost with default port",
        "127.0.0.1 (loopback) with default port",
        "[::1] (ipv6 loopback) with default port",
        "ipv4 address with default port",
        "ipv6 address with default port",
        "random hostname with the same port as server",
        "random hostname with a different port than server",
        "random hostname with default port",
        "localhost with a different port than server",
        "127.0.0.1 (loopback) with a different port than server",
        "[::1] (ipv6 loopback) with a different port than server",
        "ipv4 address with a different port than server",
        "ipv6 address with a different port than server",
    ],
)
def test_host_header(configuration, hostname, port_type, status):
    host = get_host(port_type, hostname, configuration["port"])
    response = http_request(configuration["host"], configuration["port"], host=host)

    assert response.status == status


@pytest.mark.parametrize(
    "origin, add_port, status",
    [
        (None, False, 200),
        ("", False, 500),
        ("sometext", False, 500),
        ("http://localhost", True, 500),
    ],
)
def test_origin_header(configuration, origin, add_port, status):
    if add_port:
        origin = f"{origin}:{configuration['port']}"
    response = http_request(configuration["host"], configuration["port"], origin=origin)
    assert response.status == status
