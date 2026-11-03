#!/usr/bin/python3
import json
import struct
import sys
import time
from pathlib import Path

log_path = Path(__file__).with_name('allowed-host-events.jsonl')
def log(event):
    with log_path.open('a') as stream:
        stream.write(json.dumps({'event': event, 'at': time.time() * 1000, 'pid': __import__('os').getpid()}) + '\n')
def send(value):
    data = json.dumps(value).encode('utf-8')
    sys.stdout.buffer.write(struct.pack('<I', len(data)) + data)
    sys.stdout.buffer.flush()
log('start')
while True:
    header = sys.stdin.buffer.read(4)
    if len(header) != 4:
        log('eof')
        break
    size = struct.unpack('<I', header)[0]
    value = json.loads(sys.stdin.buffer.read(size))
    if value.get('command') == 'armed-close':
        now = time.time() * 1000
        send({'armed': True, 'closeRequestedAt': now})
        time.sleep(value['delay'])
        log('delayed-exit')
        break
    if value.get('command') == 'echo':
        send(value)
