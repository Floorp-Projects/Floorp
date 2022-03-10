from http.client import HTTPConnection


def get_host(hostname, port_type, remote_agent_port):
    if port_type == "default_port":
        return hostname
    elif port_type == "remote_agent_port":
        return hostname + ":" + remote_agent_port
    elif port_type == "wrong_port":
        wrong_port = str(int(remote_agent_port) + 1)
        return hostname + ":" + wrong_port


def connect(remote_agent_port, host=None, origin=None):
    real_host = f"localhost:{remote_agent_port}"
    url = f"http://{real_host}/session"

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
