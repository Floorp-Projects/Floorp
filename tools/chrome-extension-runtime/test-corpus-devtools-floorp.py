#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Exercise unchanged signed DevTools extensions in the real native Floorp toolbox."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / 'tools/chrome-extension-runtime'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--browser', required=True)
    parser.add_argument('--ids', default='fmkadmapgofadopljbjfkapdkoienihi,lmhkpmbekcpmknklioeibfkpmmfibljd')
    parser.add_argument('--timeout', type=int, default=240)
    args = parser.parse_args()
    output = Path(tempfile.mkdtemp(prefix='corpus-devtools-floorp-', dir=ROOT / '_dist/chrome-runtime-tests'))
    fixture = output / 'fixture'
    subprocess.run(['node', str(TOOLS / 'build-devtools-fixture.mjs'), str(fixture)], check=True)
    metadata = json.loads((fixture / 'frameworks.json').read_text())

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            path = fixture / ('app.js' if self.path == '/app.js' else 'index.html') if self.path.startswith('/app') else None
            data = path.read_bytes() if path else b'Not found'
            self.send_response(200 if path else 404)
            self.send_header('Content-Type', 'application/javascript' if self.path == '/app.js' else 'text/html; charset=utf-8')
            self.send_header('Content-Length', str(len(data)))
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            self.wfile.write(data)

        def log_message(self, *args):
            pass

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    url = f'http://127.0.0.1:{server.server_address[1]}/app'
    metadata['url'] = url
    command = ['python3', str(TOOLS / 'run-browser-tests.py'), '--browser', args.browser,
               '--test', str(TOOLS / 'test-corpus-devtools-floorp.mjs'), '--timeout', str(args.timeout),
               '--actor-module', str(TOOLS / 'CorpusDevtoolsChild.sys.mjs')]
    try:
        result = subprocess.run(command, env={**os.environ, 'FLOORP_CORPUS_ROOT': str(ROOT),
            'FLOORP_CORPUS_FIXTURE': url, 'FLOORP_CORPUS_FIXTURE_METADATA': json.dumps(metadata),
            'FLOORP_CORPUS_OUTPUT': str(output), 'FLOORP_CORPUS_IDS': args.ids})
        print('REPORT ' + str(output / 'report.json'))
        return result.returncode
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


if __name__ == '__main__':
    raise SystemExit(main())
