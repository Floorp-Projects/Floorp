import json
import time
import urllib.request
import urllib.parse
import threading
import http.server
import socketserver
import socket
from pathlib import Path

BASE_URL = "http://127.0.0.1:58261"

# HTML for testing Network Idle
# It fetches multiple resources including an image and CSS.
TEST_PAGE_HTML = """
<!DOCTYPE html>
<html>
<head>
    <title>Network Idle Test</title>
    <link rel="stylesheet" href="/slow-css">
</head>
<body>
    <h1>Network Idle Test</h1>
    <img src="/slow-image">
    <div id="logs"></div>
    <script>
        // Start fetch immediately, before anything else
        fetch('/slow-resource-1').then(() => {
            console.log("Fetch 1 complete.");
            document.body.innerHTML += "<p id='done'>Done</p>";
        });
    </script>
</body>
</html>
"""

class SlowHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, format, *args):
        # Suppress logging to keep output clean
        pass

    def do_GET(self):
        if self.path == "/":
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.end_headers()
            self.wfile.write(TEST_PAGE_HTML.encode())
        elif self.path == "/slow-css":
            time.sleep(1)  # Simulate slow CSS
            self.send_response(200)
            self.send_header("Content-type", "text/css")
            self.end_headers()
            self.wfile.write(b"body { color: red; }")
        elif self.path == "/slow-image":
            time.sleep(2)  # Simulate slow image
            self.send_response(200)
            self.send_header("Content-type", "image/png")
            self.end_headers()
            # Minimal 1x1 transparent PNG
            self.wfile.write(b'\\x89PNG\\r\\n\\x1a\\n\\x00\\x00\\x00\\rIHDR\\x00\\x00\\x00\\x01\\x00\\x00\\x00\\x01\\x08\\x06\\x00\\x00\\x00\\x1f\\x15\\xc4\\x89\\x00\\x00\\x00\\nIDATx\\x9cc\\x00\\x01\\x00\\x00\\x05\\x00\\x01\\r\\n\\x2e\\xae\\x00\\x00\\x00\\x00IEND\\xaeB`\\x82')
        elif self.path == "/slow-resource-1":
            time.sleep(3)  # Simulate slow resource
            self.send_response(200)
            self.send_header("Content-type", "text/plain")
            self.end_headers()
            self.wfile.write(b"Slow resource 1 content")
        elif self.path == "/slow-resource-2":
            time.sleep(1)  # Simulate another slow resource
            self.send_response(200)
            self.send_header("Content-type", "text/plain")
            self.end_headers()
            self.wfile.write(b"Slow resource 2 content")
        else:
            self.send_error(404)

class ThreadedTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    pass

def run_server(port):
    # Use allow_reuse_address to avoid "Address already in use" errors on retry
    socketserver.TCPServer.allow_reuse_address = True
    with ThreadedTCPServer(("127.0.0.1", port), SlowHandler) as httpd:
        httpd.serve_forever()

def make_request(path, method="GET", data=None):
    url = f"{BASE_URL}{path}"
    req = urllib.request.Request(url, method=method)
    if data:
        data = json.dumps(data).encode()
        req.add_header("Content-Type", "application/json")
    
    try:
        with urllib.request.urlopen(req, data=data, timeout=15) as res:
            return res.status, json.loads(res.read().decode())
    except Exception as e:
        return None, str(e)

def main():
    port = 8889 # Use a different port just in case
    server_thread = threading.Thread(target=run_server, args=(port,), daemon=True)
    server_thread.start()
    
    test_url = f"http://127.0.0.1:{port}/"
    print(f"Testing Network Idle with {test_url}")
    
    # 1. Create instance
    print("Creating instance...")
    status, body = make_request("/scraper/instances", "POST")
    if status != 200:
        print(f"Failed to create instance: {body}")
        return
    instance_id = body["instanceId"]
    print(f"Created instance: {instance_id}")
    
    try:
        # 2. Navigate
        print("Navigating...")
        nav_status, nav_body = make_request(f"/scraper/instances/{instance_id}/navigate", "POST", {"url": test_url})
        if nav_status != 200:
            print(f"Navigation failed: {nav_body}")
            return
        
        # 3. Wait for Network Idle
        # The page will be idle after 1s (delay) + 2s (fetch) = 3s.
        # We call waitForNetworkIdle immediately.
        print("Calling waitForNetworkIdle (should take ~3s)...")
        start_time = time.time()
        status, body = make_request(f"/scraper/instances/{instance_id}/waitForNetworkIdle", "POST", {"timeout": 10000})
        end_time = time.time()
        
        duration = end_time - start_time
        print(f"waitForNetworkIdle returned after {duration:.2f}s")
        print(f"Result: {body}")
        
        # 0.5s idle threshold + 3s max delay = 3.5s total expected
        if body.get("ok") and 2.5 <= duration <= 6.0:
            print("SUCCESS: waitForNetworkIdle correctly waited for background network activity.")
        else:
            print(f"FAILURE: waitForNetworkIdle took {duration:.2f}s, which is outside expected range (2.5-6.0s).")

        # 4. Immediate call when idle
        print("Calling waitForNetworkIdle again (should be immediate)...")
        start_time = time.time()
        status, body = make_request(f"/scraper/instances/{instance_id}/waitForNetworkIdle", "POST", {"timeout": 10000})
        end_time = time.time()
        duration = end_time - start_time
        print(f"waitForNetworkIdle returned after {duration:.2f}s")
        if body.get("ok") and duration < 1.0:
            print("SUCCESS: waitForNetworkIdle returned quickly when already idle.")
        else:
            print("FAILURE: waitForNetworkIdle took too long when already idle.")

    finally:
        print("Destroying instance...")
        make_request(f"/scraper/instances/{instance_id}", "DELETE")

if __name__ == "__main__":
    main()
