import socket

from requests.adapters import HTTPAdapter
from requests.compat import urlparse, unquote
try:
    from requests.packages.urllib3.connection import HTTPConnection
    from requests.packages.urllib3.connectionpool import HTTPConnectionPool
except ImportError:
    from urllib3.connection import HTTPConnection
    from urllib3.connectionpool import HTTPConnectionPool


# The following was adapted from some code from docker-py
# https://github.com/docker/docker-py/blob/master/docker/unixconn/unixconn.py
class UnixHTTPConnection(HTTPConnection):

    def __init__(self, unix_socket_url, timeout=60):
        """Create an HTTP connection to a unix domain socket

        :param unix_socket_url: A URL with a scheme of 'http+unix' and the
        netloc is a percent-encoded path to a unix domain socket. E.g.:
        'http+unix://%2Ftmp%2Fprofilesvc.sock/status/pid'
        """
        HTTPConnection.__init__(self, 'localhost', timeout=timeout)
        self.unix_socket_url = unix_socket_url
        self.timeout = timeout

    def connect(self):
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(self.timeout)
        socket_path = unquote(urlparse(self.unix_socket_url).netloc)
        sock.connect(socket_path)
        self.sock = sock


class UnixHTTPConnectionPool(HTTPConnectionPool):

    def __init__(self, socket_path, timeout=60):
        HTTPConnectionPool.__init__(self, 'localhost', timeout=timeout)
        self.socket_path = socket_path
        self.timeout = timeout

    def _new_conn(self):
        return UnixHTTPConnection(self.socket_path, self.timeout)


class UnixAdapter(HTTPAdapter):

    def __init__(self, timeout=60):
        super(UnixAdapter, self).__init__()
        self.timeout = timeout

    def get_connection(self, socket_path, proxies=None):
        proxies = proxies or {}
        proxy = proxies.get(urlparse(socket_path.lower()).scheme)

        if proxy:
            raise ValueError('%s does not support specifying proxies'
                             % self.__class__.__name__)
        return UnixHTTPConnectionPool(socket_path, self.timeout)
