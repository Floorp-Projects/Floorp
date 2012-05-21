# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import BaseHTTPServer
import json
import re
import traceback

from errors import *
from marionette import Marionette, HTMLElement

class SeleniumRequestServer(BaseHTTPServer.HTTPServer):

    def __init__(self, marionette, *args, **kwargs):
        self.marionette = marionette
        BaseHTTPServer.HTTPServer.__init__(self, *args, **kwargs)

    def __del__(self):
        if self.marionette.server:
            self.marionette.delete_session()

class SeleniumRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):

    pathRe = re.compile(r'/session/(.*?)/((element/(.*?)/)?(.*))')

    def server_error(self, error):
        self.send_response(500)
        self.send_header("Content-type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps({'status': 500, 'value': {'message': error}}))

    def file_not_found(self):
        self.send_response(404)
        self.send_header("Content-type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps({'status': 404, 'value': {'message': '%s not found' % self.path}}))

    def send_JSON(self, data=None, session=None, value=None):
        self.send_response(200)
        self.send_header("Content-type", "application/json")
        self.end_headers()

        if data is None:
            data = {}
        if not 'status' in data:
            data['status'] = 0
        if session is not None:
            data['sessionId'] = session
        if value is None:
            data['value'] = {}
        else:
            data['value'] = value

        self.wfile.write(json.dumps(data))

    def process_request(self):
        session = body = None
        path = self.path
        element = None
        m = self.pathRe.match(self.path)
        if m:
            session = m.group(1)
            element = m.group(4)
            path = '/%s' % m.group(5)
        content_len = self.headers.getheader('content-length')
        if content_len:
            body = json.loads(self.rfile.read(int(content_len)))
        return path, body, session, element

    def do_DELETE(self):
        try:

            path, body, session, element = self.process_request()

            if path == '/session':
                assert(session)
                assert(self.server.marionette.delete_session())
                self.send_JSON(session=session)
            elif path == '/window':
                assert(session)
                assert(self.server.marionette.close_window())
                self.send_JSON(session=session)
            else:
                self.file_not_found()

        except MarionetteException, e:
            self.send_JSON(data={'status': e.status}, value={'message': e.message})
        except:
            self.server_error(traceback.format_exc())

    def do_GET(self):
        try:

            path, body, session, element = self.process_request()

            if path.startswith('/attribute/'):
                assert(session)
                name = path[len('/attribute/'):]
                marionette_element = HTMLElement(self.server.marionette, element)
                self.send_JSON(session=session,
                               value=marionette_element.get_attribute(name))
            elif path == '/displayed':
                assert(session)
                marionette_element = HTMLElement(self.server.marionette, element)
                self.send_JSON(session=session,
                               value=marionette_element.displayed())
            elif path == '/enabled':
                assert(session)
                marionette_element = HTMLElement(self.server.marionette, element)
                self.send_JSON(session=session,
                               value=marionette_element.enabled())
            elif path.startswith('/equals/'):
                assert(session)
                other = path[len('/equals'):]
                marionette_element = HTMLElement(self.server.marionette, element)
                other_element = HTMLElement(self.server.marionette, other)
                self.send_JSON(session=session,
                               value=marionette_element.equals(other))
            elif path == '/selected':
                assert(session)
                marionette_element = HTMLElement(self.server.marionette, element)
                self.send_JSON(session=session,
                               value=marionette_element.selected())
            elif path == '/status':
                self.send_JSON(data=self.server.marionette.status())
            elif path == '/text':
                assert(session)
                marionette_element = HTMLElement(self.server.marionette, element)
                self.send_JSON(session=session,
                               value=marionette_element.text())
            elif path == '/url':
                assert(session)
                self.send_JSON(value=self.server.marionette.get_url(),
                               session=session)
            elif path == '/value':
                assert(session)
                marionette_element = HTMLElement(self.server.marionette, element)
                send.send_JSON(session=session,
                               value=marionette_element.value())
            elif path == '/window_handle':
                assert(session)
                self.send_JSON(session=session,
                               value=self.server.marionette.get_window())
            elif path == '/window_handles':
                assert(session)
                self.send_JSON(session=session,
                               value=self.server.marionette.get_windows())
            else:
                self.file_not_found()

        except MarionetteException, e:
            self.send_JSON(data={'status': e.status}, value={'message': e.message})
        except:
            self.server_error(traceback.format_exc())

    def do_POST(self):
        try:

            path, body, session, element = self.process_request()

            if path == '/back':
                assert(session)
                assert(self.server.marionette.go_back())
                self.send_JSON(session=session)
            elif path == '/clear':
                assert(session)
                marionette_element = HTMLElement(self.server.marionette, element)
                marionette_element.clear()
                self.send_JSON(session=session)
            elif path == '/click':
                assert(session)
                marionette_element = HTMLElement(self.server.marionette, element)
                marionette_element.click()
                self.send_JSON(session=session)
            elif path == '/element':
                # find element variants
                assert(session)
                self.send_JSON(session=session,
                               value={'ELEMENT': str(self.server.marionette.find_element(body['using'], body['value'], id=element))})
            elif path == '/elements':
                # find elements variants
                assert(session)
                self.send_JSON(session=session,
                               value=[{'ELEMENT': str(x)} for x in self.server.marionette.find_elements(body['using'], body['value'])])
            elif path == '/execute':
                assert(session)
                if body['args']:
                    result = self.server.marionette.execute_script(body['script'], script_args=body['args'])
                else:
                    result = self.server.marionette.execute_script(body['script'])
                self.send_JSON(session=session, value=result)
            elif path == '/execute_async':
                assert(session)
                if body['args']:
                    result = self.server.marionette.execute_async_script(body['script'], script_args=body['args'])
                else:
                    result = self.server.marionette.execute_async_script(body['script'])
                self.send_JSON(session=session, value=result)
            elif path == '/forward':
                assert(session)
                assert(self.server.marionette.go_forward())
                self.send_JSON(session=session)
            elif path == '/frame':
                assert(session)
                frame = body['id']
                if isinstance(frame, dict) and 'ELEMENT' in frame:
                    frame = HTMLElement(self.server.marionette, frame['ELEMENT'])
                assert(self.server.marionette.switch_to_frame(frame))
                self.send_JSON(session=session)
            elif path == '/refresh':
                assert(session)
                assert(self.server.marionette.refresh())
                self.send_JSON(session=session)
            elif path == '/session':
                session = self.server.marionette.start_session()
                # 'value' is the browser capabilities, which we're ignoring for now
                self.send_JSON(session=session, value={})
            elif path == '/timeouts/async_script':
                assert(session)
                assert(self.server.marionette.set_script_timeout(body['ms']))
                self.send_JSON(session=session)
            elif path == '/timeouts/implicit_wait':
                assert(session)
                assert(self.server.marionette.set_search_timeout(body['ms']))
                self.send_JSON(session=session)
            elif path == '/url':
                assert(session)
                assert(self.server.marionette.navigate(body['url']))
                self.send_JSON(session=session)
            elif path == '/value':
                assert(session)
                keys = ''.join(body['value'])
                marionette_element = HTMLElement(self.server.marionette, element)
                assert(marionette_element.send_keys(keys))
                self.send_JSON(session=session)
            elif path == '/window':
                assert(session)
                assert(self.server.marionette.switch_to_window(body['name']))
                self.send_JSON(session=session)
            else:
                self.file_not_found()

        except MarionetteException, e:
            self.send_JSON(data={'status': e.status}, value={'message': e.message})
        except:
            self.server_error(traceback.format_exc())

class SeleniumProxy(object):

    def __init__(self, remote_host, remote_port, proxy_port=4444):
        self.remote_host = remote_host
        self.remote_port = remote_port
        self.proxy_port = proxy_port

    def start(self):
        marionette = Marionette(self.remote_host, self.remote_port)
        httpd = SeleniumRequestServer(marionette,
                                      ('127.0.0.1', self.proxy_port),
                                      SeleniumRequestHandler)
        httpd.serve_forever()

if __name__ == "__main__":
    proxy = SeleniumProxy('localhost', 2626)
    proxy.start()
