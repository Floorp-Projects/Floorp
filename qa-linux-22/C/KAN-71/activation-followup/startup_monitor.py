#!/usr/bin/python3
"""Observe actual X11 startup ClientMessages, saving only kind and timestamp."""
import ctypes as C,json,re,sys
L=C.CDLL('libX11.so.6')
class Data(C.Union):_fields_=[('b',C.c_char*20),('s',C.c_short*10),('l',C.c_long*5)]
class Client(C.Structure):_fields_=[('type',C.c_int),('serial',C.c_ulong),('send_event',C.c_int),('display',C.c_void_p),('window',C.c_ulong),('message_type',C.c_ulong),('format',C.c_int),('data',Data)]
class Event(C.Union):_fields_=[('type',C.c_int),('client',Client),('pad',C.c_long*24)]
L.XOpenDisplay.restype=C.c_void_p;d=L.XOpenDisplay(None)
L.XDefaultRootWindow.argtypes=[C.c_void_p];L.XDefaultRootWindow.restype=C.c_ulong
L.XInternAtom.argtypes=[C.c_void_p,C.c_char_p,C.c_int];L.XInternAtom.restype=C.c_ulong
L.XSelectInput.argtypes=[C.c_void_p,C.c_ulong,C.c_long];L.XNextEvent.argtypes=[C.c_void_p,C.POINTER(Event)]
root=L.XDefaultRootWindow(d);atoms={L.XInternAtom(d,v,False):v.decode() for v in [b'_NET_STARTUP_INFO_BEGIN',b'_NET_STARTUP_INFO']}
L.XSelectInput(d,root,1<<22);buffer=b''
while True:
 e=Event();L.XNextEvent(d,C.byref(e))
 if e.type!=33 or e.client.message_type not in atoms:continue
 if atoms[e.client.message_type].endswith('BEGIN'):buffer=b''
 chunk=C.string_at(C.addressof(e.client.data),20);buffer+=chunk.split(b'\0',1)[0]
 if b'\0' in chunk:
  s=buffer.decode(errors='replace');m=re.search(r'_TIME(\d+)',s)
  with open(sys.argv[1],'a') as f:f.write(json.dumps({'kind':s.split(':',1)[0],'startupTimestamp':int(m.group(1)) if m else None,'actualX11StartupMessage':True})+'\n')
  buffer=b''
