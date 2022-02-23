import pytest

from . import Geckodriver, get_host, request


@pytest.mark.parametrize(
    "allow_hosts, hostname, port_type, status",
    [
        # Valid hosts
        (["localhost.localdomain", "localhost"], "localhost", "server_port", 200),
        (["localhost.localdomain", "localhost"], "127.0.0.1", "server_port", 200),
        # Invalid hosts
        (["localhost.localdomain"], "localhost", "server_port", 500),
        (["localhost"], "localhost", "wrong_port", 500),
        (["www.localhost"], "localhost", "server_port", 500),
    ],
)
def test_allow_hosts(configuration, allow_hosts, hostname, port_type, status):
    extra_args = ["--allow-hosts"] + allow_hosts

    with Geckodriver(configuration, hostname, extra_args) as geckodriver:
        host = get_host(port_type, hostname, geckodriver.port)
        response = request(geckodriver.hostname, geckodriver.port, host=host)

    assert response.status == status
