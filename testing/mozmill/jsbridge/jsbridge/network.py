# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Corporation Code.
#
# The Initial Developer of the Original Code is
# Mikeal Rogers.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Mikeal Rogers <mikeal.rogers@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

import asyncore
import socket
import select
import logging
import uuid
from time import sleep
from threading import Thread

try:
    import json as simplejson
    from json.encoder import encode_basestring_ascii, encode_basestring
except ImportError:
    import simplejson
    from simplejson.encoder import encode_basestring_ascii, encode_basestring

logger = logging.getLogger(__name__)

class JavaScriptException(Exception): pass

class Telnet(asyncore.dispatcher):
    def __init__(self, host, port):
        self.host, self.port = host, port
        asyncore.dispatcher.__init__(self)
        self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
        self.connect((host, port))
        self.buffer = ''
        self.logger = logger

    def __del__(self):
        self.close()

    def handle_close(self):
        """override method of asyncore.dispatcher"""
        self.close()

    def handle_expt(self): self.close() # connection failed, shutdown
    
    def writable(self):
        return (len(self.buffer) > 0)

    def handle_write(self):
        sent = self.send(self.buffer)
        self.buffer = self.buffer[sent:]
        
    def read_all(self):
        import socket
        data = ''
        while 1:
            try:
                data += self.recv(4096)
            except socket.error:
                return data

    def handle_read(self):
        self.data = self.read_all()
        self.process_read(self.data)
        
    read_callback = lambda self, data: None

decoder = simplejson.JSONDecoder()

class JSObjectEncoder(simplejson.JSONEncoder):
    """Encoder that supports jsobject references by name."""

    def encode(self, o):
        import jsobjects
        if isinstance(o, jsobjects.JSObject):
            return o._name_
        else:
            return simplejson.JSONEncoder.encode(self, o)

    def _iterencode(self, o, markers=None):
        import jsobjects
        if isinstance(o, jsobjects.JSObject):
            yield o._name_
        elif isinstance(o, basestring):
            if self.ensure_ascii:
                encoder = encode_basestring_ascii
            else:
                encoder = encode_basestring
            _encoding = self.encoding
            if (_encoding is not None and isinstance(o, str)
                    and not (_encoding == 'utf-8')):
                o = o.decode(_encoding)
            yield encoder(o)
        elif o is None:
            yield 'null'
        elif o is True:
            yield 'true'
        elif o is False:
            yield 'false'
        elif isinstance(o, (int, long)):
            yield str(o)
        elif isinstance(o, float):
            yield getattr(simplejson.encoder, 'floatstr', simplejson.encoder._floatstr)(o, self.allow_nan)
        elif isinstance(o, (list, tuple)):
            for chunk in self._iterencode_list(o, markers):
                yield chunk
        elif isinstance(o, dict):
            for chunk in self._iterencode_dict(o, markers):
                yield chunk
        else:
            if markers is not None:
                markerid = id(o)
                if markerid in markers:
                    raise ValueError("Circular reference detected")
                markers[markerid] = o
            for chunk in self._iterencode_default(o, markers):
                yield chunk
            if markers is not None:
                del markers[markerid]

encoder = JSObjectEncoder()

class JSBridgeDisconnectError(Exception): 
    """exception raised when an unexpected disconect happens"""


class Bridge(Telnet):
    
    trashes = []
    reading = False
    sbuffer = ''
    events_list = []

    callbacks = {}
        
    bridge_type = "bridge"
    
    registered = False
    timeout_ctr = 0. # global timeout counter
    
    def __init__(self, host, port, timeout=60.):
        """
        - timeout : failsafe timeout for each call to run in seconds
        """
        self.timeout = timeout
        Telnet.__init__(self, host, port)
        sleep(.1)

        # XXX we've actually already connected in Telnet
        self.connect((host, port))
    
    def handle_connect(self):
        self.register()

    def run(self, _uuid, exec_string, interval=.2, raise_exeption=True):


        exec_string += '\r\n'
        self.send(exec_string)

        while _uuid not in self.callbacks.keys():

            Bridge.timeout_ctr += interval
            if Bridge.timeout_ctr > self.timeout:
                print 'Timeout: %s' % exec_string
                raise JSBridgeDisconnectError("Connection timed out")
            
            sleep(interval)
            try:
                self.send('')
            except socket.error:
                raise JSBridgeDisconnectError("Connected disconnected")

        Bridge.timeout_ctr = 0. # reset the counter
        
        callback = self.callbacks.pop(_uuid)
        if callback['result'] is False and raise_exeption is True:
            raise JavaScriptException(callback['exception'])
        return callback 
        
    def register(self):
        _uuid = str(uuid.uuid1())
        self.send('bridge.register("'+_uuid+'", "'+self.bridge_type+'")\r\n')
        self.registered = True

    def execFunction(self, func_name, args, interval=.25):
        _uuid = str(uuid.uuid1())
        exec_args = [encoder.encode(_uuid), func_name, encoder.encode(args)]
        return self.run(_uuid, 'bridge.execFunction('+ ', '.join(exec_args)+')', interval)
        
    def setAttribute(self, obj_name, name, value):
        _uuid = str(uuid.uuid1())
        exec_args = [encoder.encode(_uuid), obj_name, encoder.encode(name), encoder.encode(value)]
        return self.run(_uuid, 'bridge.setAttribute('+', '.join(exec_args)+')')
        
    def set(self, obj_name):
        _uuid = str(uuid.uuid1())
        return self.run(_uuid, 'bridge.set('+', '.join([encoder.encode(_uuid), obj_name])+')')
        
    def describe(self, obj_name):
        _uuid = str(uuid.uuid1())
        return self.run(_uuid, 'bridge.describe('+', '.join([encoder.encode(_uuid), obj_name])+')')
    
    def fire_callbacks(self, obj):
        self.callbacks[obj['uuid']] = obj
    
    def process_read(self, data):
        """Parse out json objects and fire callbacks."""
        self.sbuffer += data
        self.reading = True
        self.parsing = True
        while self.parsing:
            # Remove erroneus data in front of callback object
            index = self.sbuffer.find('{')
            if index is not -1 and index is not 0:
                self.sbuffer = self.sbuffer[index:]
            # Try to get a json object from the data stream    
            try:
                obj, index = decoder.raw_decode(self.sbuffer)
            except Exception, e:
                self.parsing = False
            # If we got an object fire the callback infra    
            if self.parsing:
                self.fire_callbacks(obj)
                self.sbuffer = self.sbuffer[index:]
        
class BackChannel(Bridge):
    
    bridge_type = "backchannel"
    
    def __init__(self, host, port):
        Bridge.__init__(self, host, port)
        self.uuid_listener_index = {}
        self.event_listener_index = {}
        self.global_listeners = []
        
    def fire_callbacks(self, obj):
        """Handle all callback fireing on json objects pulled from the data stream."""
        self.fire_event(**dict([(str(key), value,) for key, value in obj.items()]))

    def add_listener(self, callback, uuid=None, eventType=None):
        if uuid is not None:
            self.uuid_listener_index.setdefault(uuid, []).append(callback)
        if eventType is not None:
            self.event_listener_index.setdefault(eventType, []).append(callback)

    def add_global_listener(self, callback):
        self.global_listeners.append(callback)

    def fire_event(self, eventType=None, uuid=None, result=None, exception=None):
        Bridge.timeout_ctr = 0. # reset the counter
        event = eventType
        if uuid is not None and self.uuid_listener_index.has_key(uuid):
            for callback in self.uuid_listener_index[uuid]:
                callback(result)
        if event is not None and self.event_listener_index.has_key(event):
            for callback in self.event_listener_index[event]:
                callback(result)
        for listener in self.global_listeners:
            listener(eventType, result)

thread = None
 
def create_network(hostname, port):
    back_channel = BackChannel(hostname, port)
    bridge = Bridge(hostname, port)
    global thread
    if not thread or not thread.isAlive():
        def do():
            try: asyncore.loop(use_poll=True)
            except select.error:pass
            
        thread = Thread(target=do)
        getattr(thread, 'setDaemon', lambda x : None)(True)
        thread.start()
    
    return back_channel, bridge
