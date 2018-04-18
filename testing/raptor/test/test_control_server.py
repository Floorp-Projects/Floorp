from __future__ import absolute_import, unicode_literals

import mozunit

from BaseHTTPServer import HTTPServer
from mozlog.structuredlog import set_default_logger, StructuredLogger
from raptor.control_server import RaptorControlServer

set_default_logger(StructuredLogger('test_control_server'))


def test_start_and_stop():
    control = RaptorControlServer()

    assert control.server is None
    control.start()
    assert isinstance(control.server, HTTPServer)
    assert control.server.fileno()
    assert control._server_thread.is_alive()

    control.stop()
    assert not control._server_thread.is_alive()


if __name__ == '__main__':
    mozunit.main()
