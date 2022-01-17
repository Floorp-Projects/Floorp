import socket
import subprocess
import time

from http.client import HTTPConnection


def request(server_host, server_port, path="/status", host=None, origin=None):
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


def get_free_port():
    """Get a random unbound port"""
    max_attempts = 10
    err = None
    for _ in range(max_attempts):
        s = socket.socket()
        try:
            s.bind(("127.0.0.1", 0))
        except OSError as e:
            err = e
            continue
        else:
            return s.getsockname()[1]
        finally:
            s.close()
    if err is None:
        err = Exception("Failed to get a free port")
    raise err


def get_host(port_type, hostname, server_port):
    if port_type == "default_port":
        return hostname
    if port_type == "server_port":
        return f"{hostname}:{server_port}"
    if port_type == "wrong_port":
        wrong_port = int(server_port) + 1
        return f"{hostname}:{wrong_port}"
    raise Exception(f"Unrecognised port_type {port_type}")


class Geckodriver:
    def __init__(self, configuration, hostname, extra_args):
        self.config = configuration["webdriver"]
        self.hostname = hostname
        self.extra_args = extra_args
        self.command = None
        self.proc = None
        self.port = None

    def __enter__(self):
        self.port = get_free_port()
        self.command = (
            [self.config["binary"], "--port", str(self.port)]
            + self.config["args"]
            + self.extra_args
        )
        self.proc = subprocess.Popen(
            self.command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
        )

        # Wait for the port to become ready
        end_time = time.time() + 10
        while time.time() < end_time:
            if self.proc.poll() is not None:
                raise (f"geckodriver terminated with code {self.proc.poll()}")
            with socket.socket() as sock:
                if sock.connect_ex((self.hostname, self.port)) == 0:
                    break
        else:
            raise Exception(
                f"Failed to connect to geckodriver on {self.hostname}:{self.port}"
            )

        return self

    def __exit__(self, *args, **kwargs):
        self.proc.terminate()
        print(self.proc.stdout.read())
