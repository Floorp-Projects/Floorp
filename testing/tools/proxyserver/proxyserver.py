# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Caching HTTP Proxy for use with the Talos pageload tests
Author: Rob Arnold

This file implements a multithreaded caching http 1.1 proxy. HEAD and GET
methods are supported; POST is not yet.
   
Each incoming request is put onto a new thread; python does not have a thread
pool library, so a new thread is spawned for each request. I have tried to use
the python 2.4 standard library wherever possible.

Caching:
The cache is implemented in the Cache class. Items can only be added to the
cache. The only way to remove items from the cache is to blow it all away,
either by deleting the file (default: proxy_cache.db) or passing the -c or
--clear-cache flags on the command line. It is technically possible to remove
items individually from the cache, but there has been no need to do so so far.

The cache is implemented with the shelve module. The key is the combination of
host, port and request (path + params + fragment) and the values stored are the
http status code, headers and content that were received from the remote server.

Access to the cache is guarded by a semaphore which allows concurrent read
access. The semaphore is guarded by a simple mutex which prevents a deadlock
from occuring when two threads try to add an item to the cache at the same time.

Memory usage is kept to a minimum by the shelve module; only items in the cache
that are currently being served stay in memory.

Proxy:
The BaseHTTPServer.BaseHTTPRequestHandler takes care of parsing incoming
requests and managing the socket connection. See the documentation of the
BaseHTTPServer module for more information. When do_HEAD or do_GET is called,
the url that we are supposed to fetch is in self.path.

TODO:
* Implement POST requests. This requires implementing the do_POST method and
  passing the post data along.
* Implement different cache policies
* Added an interface to allow administrators to probe the cache and remove
  items from the database and such.
"""

__version__ = "0.1"

import os
import sys
import time
import threading
import shelve
from optparse import OptionParser, OptionValueError

import SocketServer
import BaseHTTPServer
import socket
import httplib
from urlparse import urlsplit, urlunsplit

class HTTPRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  server_version = "TalosProxy/" + __version__
  protocol_version = "HTTP/1.1"

  def do_GET(self):
    content = self.send_head()
    if content:
      try:
        self.wfile.write(content)
      except socket.error, e:
        if options.verbose:
          print "Got socket error %s" % e
    #self.close_connection = 1
  def do_HEAD(self):
    self.send_head()

  def getHeaders(self):
    h = {}
    for name in self.headers.keys():
      h[name] = self.headers[name]

    return h

  def send_head(self, method="GET"): 
    o = urlsplit(self.path)

    #sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    headers = self.getHeaders()
    for k in "Proxy-Connection", "Connection":
      if k in headers:
        headers[k] = "Close"
    if "Keep-Alive" in headers:
      del headers["Keep-Alive"]

    reqstring = urlunsplit(('','',o.path, o.query, o.fragment))

    if options.no_cache:
      cache_result = None
    else:
      cache_result = cache.get(o.hostname, o.port, reqstring)

    if not cache_result:
      if options.localonly:
        self.send_error(404, "Object not in cache")
        return None
      else:
        if options.verbose:
          print "Object %s was not in the cache" % self.path
        conn = httplib.HTTPConnection(o.netloc)
        conn.request("GET", reqstring, headers=headers)
        res = conn.getresponse()

        content = res.read()
        conn.close()

        status, headers = res.status, res.getheaders()

        if not options.no_cache:
          cache.add(o.hostname, o.port, reqstring, status, headers, content)
    else:
      status, headers, content = cache_result

    try:
      self.send_response(status)
      for name, value in headers:
        # kill the transfer-encoding header because we don't support it when
        # we send data to the client
        if name not in ('transfer-encoding',):
          self.send_header(name, value)
      if "Content-Length" not in headers:
        self.send_header("Content-Length", str(len(content)))
      self.end_headers()
    except socket.error, e:
      if options.verbose:
        print "Got socket error %s" % e
      return None
    return content
  def log_message(self, format, *args):
    if options.verbose:
      BaseHTTPServer.BaseHTTPRequestHandler.log_message(self, format, *args)

class HTTPServer(SocketServer.ThreadingMixIn, BaseHTTPServer.HTTPServer):
  def __init__(self, address, handler):
    BaseHTTPServer.HTTPServer.__init__(self, address, handler)

class Cache(object):
  """Multithreaded cache uses the shelve module to store pages"""
  # 20 concurrent threads ought to be enough for one browser
  max_concurrency = 20
  def __init__(self, name='', max_concurrency=20):
    name = name or options.cache or "proxy_cache.db"
    self.name = name
    self.max_concurrency = max_concurrency
    self.entries = {}
    self.sem = threading.Semaphore(self.max_concurrency)
    self.semlock = threading.Lock()
    if options.clear_cache:
      flag = 'n'
    else:
      flag = 'c'
    self.db = shelve.DbfilenameShelf(name, flag)

  def __del__(self):
    if hasattr(self, 'db'):
      self.db.close()

  def get_key(self, host, port, resource):
    return '%s:%s/%s' % (host, port, resource)

  def get(self, host, port, resource):
    key = self.get_key(host, port, resource)
    self.semlock.acquire()
    self.sem.acquire()
    self.semlock.release()
    try:
      if not self.db.has_key(key):
        return None
      # returns status, headers, content
      return self.db[key]
    finally:
      self.sem.release()
  def add(self, host, port, resource, status, headers, content):
    key = self.get_key(host, port, resource)
    self.semlock.acquire()
    for i in range(self.max_concurrency):
      self.sem.acquire()
    self.semlock.release()
    try:
      self.db[key] = (status, headers, content)
      self.db.sync()
    finally:
      for i in range(self.max_concurrency):
        self.sem.release()

class Options(object):
  port = 8000
  localonly = False
  clear_cache = False
  no_cache = False
  cache = 'proxy_cache.db'
  verbose = False

def _parseOptions():
  def port_callback(option, opt, value, parser):
    if value > 0 and value < (2 ** 16 - 1):
      setattr(parser.values, option.dest, value)
    else:
      raise OptionValueError("Port number is out of range")

  global options
  parser = OptionParser(version="Talos Proxy " + __version__)
  parser.add_option("-p", "--port", dest="port",
    help="The port to run the proxy server on", metavar="PORT", type="int",
    action="callback", callback=port_callback)
  parser.add_option("-v", "--verbose", action="store_true", dest="verbose",
    help="Include additional debugging information")
  parser.add_option("-l", "--localonly", action="store_true", dest="localonly",
    help="Only serve pages from the local database")
  parser.add_option("-c", "--clear", action="store_true", dest="clear_cache",
    help="Clear the cache on startup")
  parser.add_option("-n", "--no-cache", action="store_true", dest="no_cache",
    help="Do not use a cache")
  parser.add_option("-u", "--use-cache", dest="cache",
    help="The filename of the cache to use", metavar="NAME.db")
  parser.set_defaults(verbose=Options.verbose,
                      port=Options.port,
                      localonly=Options.localonly,
                      clear_cache=Options.clear_cache,
                      no_cache=Options.no_cache,
                      cache=Options.cache)
  options, args = parser.parse_args()

"""Configures the proxy server. This should be called before run_proxy. It can be
called afterwards, but note that it is not threadsafe and some options (namely
port) will not take effect"""
def configure_proxy(**kwargs):
  global options
  options = Options()
  for key in kwargs:
    setattr(options, key, kwargs[key])

def _run():
  global cache
  cache = Cache()
  server_address = ('', options.port)
  httpd = HTTPServer(server_address, HTTPRequestHandler)
  httpd.serve_forever()

"""Starts the proxy; it runs on a separate daemon thread"""
def run_proxy():
  thr = threading.Thread(target=_run)
  # now when we die, the daemon thread will die too
  thr.setDaemon(1)
  thr.start()

if __name__ == '__main__':
  _parseOptions()
  try:
    run_proxy()
    # thr.join() doesn't terminate on keyboard interrupt
    while 1: time.sleep(1)
  except KeyboardInterrupt:
    if options.verbose:
      print "Quittin' time..."

__all__ = ['run_proxy', 'configure_proxy']
