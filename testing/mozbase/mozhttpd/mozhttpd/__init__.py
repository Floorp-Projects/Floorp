# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Mozhttpd is a simple http webserver written in python, designed expressly
for use in automated testing scenarios. It is designed to both serve static
content and provide simple web services.

The server is based on python standard library modules such as
SimpleHttpServer, urlparse, etc. The ThreadingMixIn is used to
serve each request on a discrete thread.

Some existing uses of mozhttpd include Peptest_, Eideticker_, and Talos_.

.. _Peptest: https://github.com/mozilla/peptest/

.. _Eideticker: https://github.com/mozilla/eideticker/

.. _Talos: http://hg.mozilla.org/build/

The following simple example creates a basic HTTP server which serves
content from the current directory, defines a single API endpoint
`/api/resource/<resourceid>` and then serves requests indefinitely:

::

  import mozhttpd

  @mozhttpd.handlers.json_response
  def resource_get(request, objid):
      return (200, { 'id': objid,
                     'query': request.query })


  httpd = mozhttpd.MozHttpd(port=8080, docroot='.',
                            urlhandlers = [ { 'method': 'GET',
                                              'path': '/api/resources/([^/]+)/?',
                                              'function': resource_get } ])
  print "Serving '%s' at %s:%s" % (httpd.docroot, httpd.host, httpd.port)
  httpd.start(block=True)

"""

from mozhttpd import MozHttpd, Request, RequestHandler, main
from handlers import json_response
