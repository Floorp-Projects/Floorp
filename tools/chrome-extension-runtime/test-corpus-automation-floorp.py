#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Run genuine frozen automation packages after production Floorp conversion."""
import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading
ROOT=Path(__file__).resolve().parents[2]
PAGE=(ROOT/'tools/chrome-extension-runtime/fixtures/corpus-automation.html').read_bytes()
class Fixture(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Type','text/html; charset=utf-8')
        self.send_header('Cache-Control','no-store')
        self.send_header('Content-Length',str(len(PAGE)))
        self.end_headers()
        self.wfile.write(PAGE)
    def log_message(self,*args):pass

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--browser',required=True)
    parser.add_argument('--timeout',type=int,default=240)
    args=parser.parse_args()
    output=Path(tempfile.mkdtemp(prefix='corpus-automation-floorp-',dir=ROOT/'_dist/chrome-runtime-tests'))
    server=ThreadingHTTPServer(('127.0.0.1',0),Fixture)
    thread=threading.Thread(target=server.serve_forever,daemon=True)
    thread.start()
    fixture=f'http://127.0.0.1:{server.server_address[1]}/automation'
    metadata={'url':fixture,'sha256':hashlib.sha256(PAGE).hexdigest()}
    try:
        result=subprocess.run(['python3',str(ROOT/'tools/chrome-extension-runtime/run-browser-tests.py'),'--browser',args.browser,'--test',str(ROOT/'tools/chrome-extension-runtime/test-corpus-automation-floorp.mjs'),'--timeout',str(args.timeout),'--actor-module',str(ROOT/'tools/chrome-extension-runtime/CorpusAutomationChild.sys.mjs')],env={**os.environ,'FLOORP_CORPUS_ROOT':str(ROOT),'FLOORP_CORPUS_FIXTURE':fixture,'FLOORP_CORPUS_FIXTURE_METADATA':json.dumps(metadata),'FLOORP_CORPUS_OUTPUT':str(output)})
        print('REPORT '+str(output/'report.json'))
        return result.returncode
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)
if __name__=='__main__':raise SystemExit(main())
