# Copyright 2011, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


"""PythonHeaderParserHandler for mod_pywebsocket.

Apache HTTP Server and mod_python must be configured such that this
function is called to handle WebSocket request.
"""


import logging

from mod_python import apache

from mod_pywebsocket import dispatch
from mod_pywebsocket import handshake
from mod_pywebsocket import util


# PythonOption to specify the handler root directory.
_PYOPT_HANDLER_ROOT = 'mod_pywebsocket.handler_root'

# PythonOption to specify the handler scan directory.
# This must be a directory under the root directory.
# The default is the root directory.
_PYOPT_HANDLER_SCAN = 'mod_pywebsocket.handler_scan'

# PythonOption to specify to allow draft75 handshake.
# The default is None (Off)
_PYOPT_ALLOW_DRAFT75 = 'mod_pywebsocket.allow_draft75'


class ApacheLogHandler(logging.Handler):
    """Wrapper logging.Handler to emit log message to apache's error.log."""

    _LEVELS = {
        logging.DEBUG: apache.APLOG_DEBUG,
        logging.INFO: apache.APLOG_INFO,
        logging.WARNING: apache.APLOG_WARNING,
        logging.ERROR: apache.APLOG_ERR,
        logging.CRITICAL: apache.APLOG_CRIT,
        }

    def __init__(self, request=None):
        logging.Handler.__init__(self)
        self.log_error = apache.log_error
        if request is not None:
            self.log_error = request.log_error

    def emit(self, record):
        apache_level = apache.APLOG_DEBUG
        if record.levelno in ApacheLogHandler._LEVELS:
            apache_level = ApacheLogHandler._LEVELS[record.levelno]

        # "server" parameter must be passed to have "level" parameter work.
        # If only "level" parameter is passed, nothing shows up on Apache's
        # log. However, at this point, we cannot get the server object of the
        # virtual host which will process WebSocket requests. The only server
        # object we can get here is apache.main_server. But Wherever (server
        # configuration context or virtual host context) we put
        # PythonHeaderParserHandler directive, apache.main_server just points
        # the main server instance (not any of virtual server instance). Then,
        # Apache follows LogLevel directive in the server configuration context
        # to filter logs. So, we need to specify LogLevel in the server
        # configuration context. Even if we specify "LogLevel debug" in the
        # virtual host context which actually handles WebSocket connections,
        # DEBUG level logs never show up unless "LogLevel debug" is specified
        # in the server configuration context.
        #
        # TODO(tyoshino): Provide logging methods on request object. When
        # request is mp_request object (when used together with Apache), the
        # methods call request.log_error indirectly. When request is
        # _StandaloneRequest, the methods call Python's logging facility which
        # we create in standalone.py.
        self.log_error(record.getMessage(), apache_level, apache.main_server)


_LOGGER = logging.getLogger('mod_pywebsocket')
# Logs are filtered by Apache based on LogLevel directive in Apache
# configuration file. We must just pass logs for all levels to
# ApacheLogHandler.
_LOGGER.setLevel(logging.DEBUG)
_LOGGER.addHandler(ApacheLogHandler())


def _create_dispatcher():
    _HANDLER_ROOT = apache.main_server.get_options().get(
            _PYOPT_HANDLER_ROOT, None)
    if not _HANDLER_ROOT:
        raise Exception('PythonOption %s is not defined' % _PYOPT_HANDLER_ROOT,
                        apache.APLOG_ERR)
    _HANDLER_SCAN = apache.main_server.get_options().get(
            _PYOPT_HANDLER_SCAN, _HANDLER_ROOT)
    dispatcher = dispatch.Dispatcher(_HANDLER_ROOT, _HANDLER_SCAN)
    for warning in dispatcher.source_warnings():
        apache.log_error('mod_pywebsocket: %s' % warning, apache.APLOG_WARNING)
    return dispatcher


# Initialize
_dispatcher = _create_dispatcher()


def headerparserhandler(request):
    """Handle request.

    Args:
        request: mod_python request.

    This function is named headerparserhandler because it is the default
    name for a PythonHeaderParserHandler.
    """

    handshake_is_done = False
    try:
        allowDraft75 = apache.main_server.get_options().get(
            _PYOPT_ALLOW_DRAFT75, None)
        handshake.do_handshake(
            request, _dispatcher, allowDraft75=allowDraft75)
        handshake_is_done = True
        request.log_error(
            'mod_pywebsocket: resource: %r' % request.ws_resource,
            apache.APLOG_DEBUG)
        request._dispatcher = _dispatcher
        _dispatcher.transfer_data(request)
    except dispatch.DispatchException, e:
        request.log_error('mod_pywebsocket: %s' % e, apache.APLOG_WARNING)
        if not handshake_is_done:
            return e.status
    except handshake.AbortedByUserException, e:
        request.log_error('mod_pywebsocket: %s' % e, apache.APLOG_INFO)
    except handshake.HandshakeException, e:
        # Handshake for ws/wss failed.
        # The request handling fallback into http/https.
        request.log_error('mod_pywebsocket: %s' % e, apache.APLOG_INFO)
        return e.status
    except Exception, e:
        request.log_error('mod_pywebsocket: %s' % e, apache.APLOG_WARNING)
        # Unknown exceptions before handshake mean Apache must handle its
        # request with another handler.
        if not handshake_is_done:
            return apache.DECLINE
    # Set assbackwards to suppress response header generation by Apache.
    request.assbackwards = 1
    return apache.DONE  # Return DONE such that no other handlers are invoked.


# vi:sts=4 sw=4 et
