"""Single-attempt bounded Marionette frames; never continue after a timed-out read."""
import json
import select
import socket
import time

MAX_FRAME = 2 * 1024 * 1024
COMMANDS = {'WebDriver:NewSession', 'Marionette:SetContext', 'WebDriver:SetTimeouts',
            'WebDriver:ExecuteAsyncScript', 'WebDriver:ExecuteScript'}
NATIVE_ERROR_CODES = {'javascript error', 'script timeout', 'timeout', 'invalid session id',
                      'session not created', 'invalid argument', 'unknown command', 'unknown error',
                      'invalid element state', 'no such window', 'unsupported operation'}
QA_SYMBOLS = {'restartOracle', 'cfg', 'snapshot', 'Services', 'ChromeUtils', 'Ci',
              'SessionStore', 'failureSnapshot', 'syntheticProfileVerified'}


def close_exception_type(error):
    try:
        name = type(error).__name__
        return name if name in {'OSError', 'ValueError', 'TypeError', 'RuntimeError'} else 'OtherException'
    except BaseException: return 'OtherException'


def native_error_summary(value):
    """No arbitrary native message, stack, response body, URI or credential text."""
    if not isinstance(value, dict):
        return {'code': 'UNRECOGNIZED', 'detail': None, 'detailWithheld': True}
    code = value.get('error')
    code = code if isinstance(code, str) and code in NATIVE_ERROR_CODES else 'UNRECOGNIZED'
    detail = None
    message = value.get('message')
    if isinstance(message, str) and len(message) <= 512:
        for symbol in QA_SYMBOLS:
            if message in [symbol + ' is not defined', 'ReferenceError: ' + symbol + ' is not defined']:
                detail = 'ReferenceError: ' + symbol + ' is not defined'
                break
    return {'code': code, 'detail': detail, 'detailWithheld': detail is None}

class BrowserPhaseError(ValueError):
    def __init__(self, result):
        self.result = result
        message=result.get('error') if isinstance(result,dict) else None
        super().__init__('Original browser phase failed: '+(message if isinstance(message,str) else 'Malformed native phase result'))

class Marionette:
    def __init__(self, port, pump, trace, failure_sink=None):
        self.pump = pump; self.trace = trace; self.index = 0; self.buffer = bytearray(); self.pending_phase_trace = None
        self.failure_sink = failure_sink
        self.sock = None; self.rpc_state = {'stage': 'connect', 'command': None, 'requestId': None,
                                           'bytesSent': 0, 'receivedBytes': 0, 'frameBytes': None,
                                           'responseCorrelated': None, 'nativeError': None}
        try:
            self.sock = socket.create_connection(('127.0.0.1', port), timeout=5)
            self.sock.setblocking(False)
            self.rpc_state['stage'] = 'handshake'
            hello = self.read(time.monotonic()+15, False)
            if not isinstance(hello,dict) or type(hello.get('marionetteProtocol')) is not int or hello['marionetteProtocol']!=3:
                raise ValueError('Unexpected original Marionette handshake')
            self.discard_phase_trace()
            self.trace(b'{"rpc":"handshake","protocol":3}')
            self.command('WebDriver:NewSession',{},90)
            self.command('Marionette:SetContext',{'value':'chrome'})
            self.command('WebDriver:SetTimeouts',{'script':80000})
        except BaseException as error:
            self.remember_failure(error)
            self.discard_phase_trace(); self.close_after_failure(error); raise
    def close(self):
        if self.sock is not None: self.sock.close()
    def close_after_failure(self, original):
        try: self.close()
        except BaseException as error:
            name=close_exception_type(error)
            self.state(secondaryCloseExceptionType=name)
            self.remember_failure(original)
            try:original.qa3_rpc_close_failure = name
            except BaseException:pass
    def remember_failure(self, error):
        sink=getattr(self,'failure_sink',None)
        if sink is not None:
            try:sink(error,dict(getattr(self,'rpc_state',{})))
            except BaseException:
                # Scope.guard still records the original with metadata unavailable.
                pass
        try:
            error.qa3_rpc_failure = dict(getattr(self, 'rpc_state', {}))
        except BaseException:pass
    def state(self, **values):
        if not hasattr(self, 'rpc_state'): self.rpc_state = {}
        self.rpc_state.update(values)
    def accept_phase_trace(self):
        data=self.pending_phase_trace;self.pending_phase_trace=None
        if data is not None:self.trace(data)
    def discard_phase_trace(self):self.pending_phase_trace=None
    def read(self, deadline, trace_response=False):
        while True:
            colon=self.buffer.find(b':')
            if colon>=0:
                header=bytes(self.buffer[:colon])
                if not header or len(header)>9 or not header.isdigit(): raise ValueError('Malformed original frame header')
                length=int(header)
                if length<=0 or colon+1+length>MAX_FRAME: raise ValueError('Original frame outside byte bound')
                self.state(frameBytes=colon+1+length)
                if len(self.buffer)>=colon+1+length:
                    data=bytes(self.buffer[colon+1:colon+1+length]);del self.buffer[:colon+1+length]
                    if trace_response:self.trace(json.dumps({'rpc':'frame','bytes':len(data)}).encode())
                    else:self.pending_phase_trace=data
                    return json.loads(data.decode('utf-8'))
            elif len(self.buffer)>9: raise ValueError('Original frame header overflow')
            if time.monotonic()>=deadline: raise TimeoutError('Original Marionette reply deadline')
            self.pump()
            if select.select([self.sock],[],[],0.05)[0]:
                data=self.sock.recv(min(65536,MAX_FRAME-len(self.buffer)))
                if not data: raise EOFError('Original Marionette peer closed before reply')
                self.buffer.extend(data)
                self.state(receivedBytes=len(self.buffer))
    def command(self, command, args=None, seconds=30, trace_response=True):
        ident=self.index;self.index+=1
        self.state(stage='write', command=command if command in COMMANDS else 'OTHER_COMMAND', requestId=ident,
                   bytesSent=0, receivedBytes=len(self.buffer), frameBytes=None,
                   responseCorrelated=None, nativeError=None)
        deadline=time.monotonic()+seconds;offset=0
        try:
            raw=json.dumps([0,ident,command,args or {}]).encode();packet=str(len(raw)).encode()+b':'+raw
            if len(packet)>MAX_FRAME:raise ValueError('Request byte bound')
            while offset<len(packet):
                if time.monotonic()>=deadline:raise TimeoutError('Original Marionette write deadline')
                self.pump()
                if select.select([],[self.sock],[],0.05)[1]:
                    size=self.sock.send(packet[offset:])
                    if size<=0:raise EOFError('Original Marionette write closed')
                    offset+=size
                    self.state(bytesSent=offset)
            self.state(stage='reply')
            response=self.read(deadline,False)
            if not isinstance(response,list) or len(response)!=4 or type(response[0]) is not int or response[0]!=1 or type(response[1]) is not int or response[1]!=ident:
                self.state(responseCorrelated=False)
                raise ValueError('Original Marionette reply identity mismatch')
            self.state(responseCorrelated=True)
            if response[2] is not None:
                summary = native_error_summary(response[2])
                self.state(stage='native-error', nativeError=summary)
                raise ValueError('Original Marionette error: '+summary['code'] if trace_response else 'Original Marionette phase command error')
            if trace_response:
                self.discard_phase_trace()
                self.trace(json.dumps({'rpc': 'response', 'requestId': ident,
                                       'command': self.rpc_state['command'], 'status': 'ok'}).encode())
            return response[3]
        except BaseException as error:
            self.remember_failure(error)
            self.discard_phase_trace(); self.close_after_failure(error); raise
    def phase(self, script, args):
        if getattr(self,'pending_phase_trace',None) is not None:raise ValueError('Previous native phase trace was not decided')
        try:
            value=self.command('WebDriver:ExecuteAsyncScript',{'script':script,'args':[args]},90,trace_response=False)
            self.state(stage='phase-result')
            if not isinstance(value,dict) or 'value' not in value:raise ValueError('Missing original browser phase result')
            result=value['value']
            if not isinstance(result,dict) or result.get('ok') is not True:raise BrowserPhaseError(result)
            return result
        except BrowserPhaseError:
            raise
        except BaseException as error:
            self.remember_failure(error)
            self.discard_phase_trace(); self.close_after_failure(error); raise
    def schedule_quit(self, workspace_exit=False):
        action="win.gBrowser.removeTab(win.gBrowser.selectedTab,{animate:false});" if workspace_exit else "Services.startup.quit(Components.interfaces.nsIAppStartup.eAttemptQuit);"
        script="const win=Services.wm.getMostRecentWindow('navigator:browser');win.setTimeout(()=>{"+action+"},500);return {scheduled:true};"
        value=self.command('WebDriver:ExecuteScript',{'script':script,'args':[]})
        if not isinstance(value,dict) or value.get('value',{}).get('scheduled') is not True:raise ValueError('Quit request was not acknowledged')
