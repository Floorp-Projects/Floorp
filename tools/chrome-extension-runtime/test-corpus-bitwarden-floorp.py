#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Run the unchanged Bitwarden package against the same genuine local test vault."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import hashlib
import os
from pathlib import Path
import ssl
import subprocess
import threading
from urllib.parse import urlparse

ROOT=Path(__file__).resolve().parents[2]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--browser',required=True)
    parser.add_argument('--reference',type=Path,help='A successful report from test-corpus-bitwarden.mjs')
    args=parser.parse_args()
    output=args.server.resolve().parent
    reference_path=(args.reference or output/'chrome-primary-reference/report.json').resolve()
    reference=json.loads(reference_path.read_text())
    if reference.get('passed') is not True: raise ValueError('A real Chrome primary-operation reference is required first')
    url=reference['authoredURL']
    fixture=(ROOT/'tools/chrome-extension-runtime/fixtures/corpus-bitwarden-login.html').read_bytes()
    fixture_hash=hashlib.sha256(fixture).hexdigest()
    if fixture_hash != reference['authoredSourceSha256']: raise ValueError('The authored Chrome/Floorp fixture bytes must match')
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            self.send_response(200);self.send_header('Content-Type','text/html; charset=utf-8');self.send_header('Cache-Control','no-store');self.end_headers();self.wfile.write(fixture)
        def log_message(self,*args): pass
    server=ThreadingHTTPServer(('127.0.0.1',urlparse(url).port),Handler)
    context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(output/'data/tls/authored.pem',output/'data/tls/authored-key.pem')
    server.socket=context.wrap_socket(server.socket,server_side=True)
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    try:
        return subprocess.run(['python3',str(ROOT/'tools/chrome-extension-runtime/run-browser-tests.py'),'--browser',args.browser,'--test',str(ROOT/'tools/chrome-extension-runtime/test-corpus-bitwarden-floorp.mjs'),'--actor-module',str(ROOT/'tools/chrome-extension-runtime/CorpusBitwardenChild.sys.mjs'),'--headed','--timeout','1800'],env={**os.environ,'FLOORP_CORPUS_ROOT':str(ROOT),'FLOORP_BITWARDEN_OUTPUT':str(output),'FLOORP_BITWARDEN_AUTHORED_URL':url,'FLOORP_BITWARDEN_FIXTURE_SHA':fixture_hash,'FLOORP_BITWARDEN_REFERENCE':str(reference_path)}).returncode
    finally:
        server.shutdown();server.server_close();thread.join(timeout=5)


if __name__=='__main__': raise SystemExit(main())
