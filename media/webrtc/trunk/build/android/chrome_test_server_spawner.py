# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A "Test Server Spawner" that handles killing/stopping per-test test servers.

It's used to accept requests from the device to spawn and kill instances of the
chrome test server on the host.
"""

import BaseHTTPServer
import logging
import os
import sys
import threading
import time
import urlparse

# Path that are needed to import testserver
cr_src = os.path.join(os.path.abspath(os.path.dirname(__file__)), '..', '..')
sys.path.append(os.path.join(cr_src, 'third_party'))
sys.path.append(os.path.join(cr_src, 'third_party', 'tlslite'))
sys.path.append(os.path.join(cr_src, 'third_party', 'pyftpdlib', 'src'))
sys.path.append(os.path.join(os.path.abspath(os.path.dirname(__file__)), '..',
   '..', 'net', 'tools', 'testserver'))
import testserver

_test_servers = []

class SpawningServerRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  """A handler used to process http GET request.
  """

  def GetServerType(self, server_type):
    """Returns the server type to use when starting the test server.

    This function translate the command-line argument into the appropriate
    numerical constant.
    # TODO(yfriedman): Do that translation!
    """
    if server_type:
      pass
    return 0

  def do_GET(self):
    parsed_path = urlparse.urlparse(self.path)
    action = parsed_path.path
    params = urlparse.parse_qs(parsed_path.query, keep_blank_values=1)
    logging.info('Action is: %s' % action)
    if action == '/killserver':
      # There should only ever be one test server at a time. This may do the
      # wrong thing if we try and start multiple test servers.
      _test_servers.pop().Stop()
    elif action == '/start':
      logging.info('Handling request to spawn a test webserver')
      for param in params:
        logging.info('%s=%s' % (param, params[param][0]))
      s_type = 0
      doc_root = None
      if 'server_type' in params:
        s_type = self.GetServerType(params['server_type'][0])
      if 'doc_root' in params:
        doc_root = params['doc_root'][0]
      self.webserver_thread = threading.Thread(
          target=self.SpawnTestWebServer, args=(s_type, doc_root))
      self.webserver_thread.setDaemon(True)
      self.webserver_thread.start()
    self.send_response(200, 'OK')
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><head><title>started</title></head></html>')
    logging.info('Returned OK!!!')

  def SpawnTestWebServer(self, s_type, doc_root):
    class Options(object):
      log_to_console = True
      server_type = s_type
      port = self.server.test_server_port
      data_dir = doc_root or 'chrome/test/data'
      file_root_url = '/files/'
      cert = False
      policy_keys = None
      policy_user = None
      startup_pipe = None
    options = Options()
    logging.info('Listening on %d, type %d, data_dir %s' % (options.port,
        options.server_type, options.data_dir))
    testserver.main(options, None, server_list=_test_servers)
    logging.info('Test-server has died.')


class SpawningServer(object):
  """The class used to start/stop a http server.
  """

  def __init__(self, test_server_spawner_port, test_server_port):
    logging.info('Creating new spawner %d', test_server_spawner_port)
    self.server = testserver.StoppableHTTPServer(('', test_server_spawner_port),
                                                 SpawningServerRequestHandler)
    self.port = test_server_spawner_port
    self.server.test_server_port = test_server_port

  def Listen(self):
    logging.info('Starting test server spawner')
    self.server.serve_forever()

  def Start(self):
    listener_thread = threading.Thread(target=self.Listen)
    listener_thread.setDaemon(True)
    listener_thread.start()
    time.sleep(1)

  def Stop(self):
    self.server.Stop()
