"""Single-attempt bounded Marionette frames; never continue after a timed-out read."""
import json
import select
import socket
import time

MAX_FRAME = 2 * 1024 * 1024

class BrowserPhaseError(ValueError):
    def __init__(self, result):
        self.result = result
        message=result.get('error') if isinstance(result,dict) else None
        super().__init__('Original browser phase failed: '+(message if isinstance(message,str) else 'Malformed native phase result'))

class Marionette:
    def __init__(self, port, pump, trace):
        self.pump = pump; self.trace = trace; self.index = 0; self.buffer = bytearray(); self.pending_phase_trace = None
        self.sock = socket.create_connection(('127.0.0.1', port), timeout=5)
        self.sock.setblocking(False)
        try:
            hello = self.read(time.monotonic()+15)
            if not isinstance(hello,dict) or type(hello.get('marionetteProtocol')) is not int or hello['marionetteProtocol']!=3:
                raise ValueError('Unexpected original Marionette handshake')
            self.command('WebDriver:NewSession',{},90)
            self.command('Marionette:SetContext',{'value':'chrome'})
            self.command('WebDriver:SetTimeouts',{'script':80000})
        except BaseException:
            self.close(); raise
    def close(self): self.sock.close()
    def accept_phase_trace(self):
        data=self.pending_phase_trace;self.pending_phase_trace=None
        if data is not None:self.trace(data)
    def discard_phase_trace(self):self.pending_phase_trace=None
    def read(self, deadline, trace_response=True):
        while True:
            colon=self.buffer.find(b':')
            if colon>=0:
                header=bytes(self.buffer[:colon])
                if not header or len(header)>9 or not header.isdigit(): raise ValueError('Malformed original frame header')
                length=int(header)
                if length<=0 or colon+1+length>MAX_FRAME: raise ValueError('Original frame outside byte bound')
                if len(self.buffer)>=colon+1+length:
                    data=bytes(self.buffer[colon+1:colon+1+length]);del self.buffer[:colon+1+length]
                    if trace_response:self.trace(data)
                    else:self.pending_phase_trace=data
                    return json.loads(data.decode('utf-8'))
            elif len(self.buffer)>9: raise ValueError('Original frame header overflow')
            if time.monotonic()>=deadline: raise TimeoutError('Original Marionette reply deadline')
            self.pump()
            if select.select([self.sock],[],[],0.05)[0]:
                data=self.sock.recv(min(65536,MAX_FRAME-len(self.buffer)))
                if not data: raise EOFError('Original Marionette peer closed before reply')
                self.buffer.extend(data)
    def command(self, command, args=None, seconds=30, trace_response=True):
        ident=self.index;self.index+=1
        raw=json.dumps([0,ident,command,args or {}]).encode();packet=str(len(raw)).encode()+b':'+raw
        if len(packet)>MAX_FRAME:raise ValueError('Request byte bound')
        deadline=time.monotonic()+seconds;offset=0
        try:
            while offset<len(packet):
                if time.monotonic()>=deadline:raise TimeoutError('Original Marionette write deadline')
                self.pump()
                if select.select([],[self.sock],[],0.05)[1]:
                    size=self.sock.send(packet[offset:])
                    if size<=0:raise EOFError('Original Marionette write closed')
                    offset+=size
            response=self.read(deadline,trace_response)
            if not isinstance(response,list) or len(response)!=4 or type(response[0]) is not int or response[0]!=1 or type(response[1]) is not int or response[1]!=ident:
                raise ValueError('Original Marionette reply identity mismatch')
            if response[2] is not None:
                raise ValueError('Original Marionette error: '+json.dumps(response[2]) if trace_response else 'Original Marionette phase command error')
            return response[3]
        except BaseException:
            self.close();raise
    def phase(self, script, args):
        if getattr(self,'pending_phase_trace',None) is not None:raise ValueError('Previous native phase trace was not decided')
        value=self.command('WebDriver:ExecuteAsyncScript',{'script':script,'args':[args]},90,trace_response=False)
        if not isinstance(value,dict) or 'value' not in value:raise ValueError('Missing original browser phase result')
        result=value['value']
        if not isinstance(result,dict) or result.get('ok') is not True:raise BrowserPhaseError(result)
        return result
    def schedule_quit(self, workspace_exit=False):
        action="win.gBrowser.removeTab(win.gBrowser.selectedTab,{animate:false});" if workspace_exit else "Services.startup.quit(Components.interfaces.nsIAppStartup.eAttemptQuit);"
        script="const win=Services.wm.getMostRecentWindow('navigator:browser');win.setTimeout(()=>{"+action+"},500);return {scheduled:true};"
        value=self.command('WebDriver:ExecuteScript',{'script':script,'args':[]})
        if not isinstance(value,dict) or value.get('value',{}).get('scheduled') is not True:raise ValueError('Quit request was not acknowledged')
