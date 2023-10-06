from http.client import HTTPConnection


def websocket_request(
    remote_agent_host, remote_agent_port, host=None, origin=None, path="/session"
):
    real_host = f"{remote_agent_host}:{remote_agent_port}"
    url = f"http://{real_host}{path}"

    conn = HTTPConnection(real_host)

    skip_host = host is not None
    conn.putrequest("GET", url, skip_host)

    if host is not None:
        conn.putheader("Host", host)

    if origin is not None:
        conn.putheader("Origin", origin)

    conn.putheader("Connection", "upgrade")
    conn.putheader("Upgrade", "websocket")
    conn.putheader("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==")
    conn.putheader("Sec-WebSocket-Version", "13")

    conn.endheaders()

    return conn.getresponse()


def http_request(server_host, server_port, path="/status", host=None, origin=None):
    url = f"http://{server_host}:{server_port}{path}"

    conn = HTTPConnection(server_host, server_port)

    custom_host = host is not None
    conn.putrequest("GET", url, skip_host=custom_host)
    if custom_host:
        conn.putheader("Host", host)

    if origin is not None:
        conn.putheader("Origin", origin)

    conn.endheaders()

    return conn.getresponse()


def get_host(port_type, hostname, server_port):
    if port_type == "default_port":
        return hostname
    if port_type == "server_port":
        return f"{hostname}:{server_port}"
    if port_type == "wrong_port":
        wrong_port = int(server_port) + 1
        return f"{hostname}:{wrong_port}"
    raise Exception(f"Unrecognised port_type {port_type}")
