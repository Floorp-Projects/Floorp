"""Read bounded, original clean Mozilla session files; never write restored state."""
import json
import struct

LIMIT = 4 * 1024 * 1024

def decode(data):
    if len(data)>LIMIT or len(data)<12 or data[:8]!=b'mozLz40\0':raise ValueError('Original clean session format/byte bound')
    size=struct.unpack('<I',data[8:12])[0]
    if size<=0 or size>LIMIT:raise ValueError('Original session expanded bound')
    source=data[12:];index=0;out=bytearray()
    def length(value):
        nonlocal index
        if value==15:
            while True:
                if index>=len(source):raise ValueError('Truncated original LZ4 length')
                n=source[index];index+=1;value+=n
                if value>LIMIT:raise ValueError('Original LZ4 length bound')
                if n!=255:break
        return value
    while index<len(source):
        token=source[index];index+=1;literals=length(token>>4)
        if index+literals>len(source) or len(out)+literals>size:raise ValueError('Original LZ4 literals outside bound')
        out.extend(source[index:index+literals]);index+=literals
        if index==len(source):break
        if index+2>len(source):raise ValueError('Truncated original LZ4 offset')
        offset=source[index]|source[index+1]<<8;index+=2;match=length(token&15)+4
        if offset<=0 or offset>len(out) or len(out)+match>size:raise ValueError('Original LZ4 match outside bound')
        while match:
            chunk=out[-offset:][:match];out.extend(chunk);match-=len(chunk)
    if len(out)!=size:raise ValueError('Original LZ4 expanded size mismatch')
    return json.loads(out.decode('utf-8'))

def saved_tabs(state):
    windows=state.get('windows',[])+state.get('_closedWindows',[])
    return [tab for win in windows for tab in win.get('tabs',[])]

def validate_saved(state, expected):
    tabs=saved_tabs(state)
    required=expected['state']['tabs'] if expected['case']=='normal' else [{'url':expected['retainedURL'],'workspace':expected['b']}]
    for want in required:
        matches=[tab for tab in tabs if any(entry.get('url')==want['url'] for entry in tab.get('entries',[])) and tab.get('floorpWorkspaceId')==want['workspace']]
        if len(matches)!=1:raise ValueError('Original clean session lacks exact saved URL/workspace')
    return True
