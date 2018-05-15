from __future__ import absolute_import, unicode_literals

import mozunit
import os
import sys

from BaseHTTPServer import HTTPServer
from mozlog.structuredlog import set_default_logger, StructuredLogger
from raptor.control_server import RaptorControlServer

# need this so the raptor unit tests can find output & filter classes
here = os.path.abspath(os.path.dirname(__file__))
raptor_dir = os.path.join(os.path.dirname(here), 'raptor')
sys.path.insert(0, raptor_dir)

from raptor.results import RaptorResultsHandler


set_default_logger(StructuredLogger('test_control_server'))


def test_start_and_stop():

    results_handler = RaptorResultsHandler()
    control = RaptorControlServer(results_handler)

    assert control.server is None
    control.start()
    assert isinstance(control.server, HTTPServer)
    assert control.server.fileno()
    assert control._server_thread.is_alive()

    control.stop()
    assert not control._server_thread.is_alive()


if __name__ == '__main__':
    mozunit.main()
