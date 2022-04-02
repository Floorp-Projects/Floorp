import pytest

from support.network import get_host, http_request


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
def test_allow_hosts(geckodriver, allow_hosts, hostname, port_type, status):
    extra_args = ["--allow-hosts"] + allow_hosts

    driver = geckodriver(hostname=hostname, extra_args=extra_args)
    host = get_host(port_type, hostname, driver.port)
    response = http_request(driver.hostname, driver.port, host=host)

    assert response.status == status
