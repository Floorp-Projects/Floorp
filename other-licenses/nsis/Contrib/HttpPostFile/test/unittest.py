# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

# Unit test for the HttpPostFile plugin, using a server on localhost.
#
# This test has not been set up to run in continuous integration. It is
# intended to be run manually, and only on Windows.
#
# Requires postdriver.exe, which can be built from postdriver.nsi with makensis
# from MozillaBuild:
#
#   makensis-3.01.exe postdriver.nsi
#
# It can then be run from this directory as:
#
#   python3 test.py

import os
import subprocess
import http.server
import socketserver
import threading

DRIVER_EXE_FILE_NAME = "postdriver.exe"
JSON_FILE_NAME = "test1.json"
RESULT_FILE_NAME = "result.txt"
BIND_HOST = "127.0.0.1"
BIND_PORT = 8080
COMMON_URL = f"http://{BIND_HOST}:{BIND_PORT}/submit"
COMMON_JSON_BYTES = '{"yes": "indeed",\n"and": "ĳ"}'.encode('utf-8')

DRIVER_TIMEOUT_SECS = 60
SERVER_TIMEOUT_SECS = 120


class PostHandler(http.server.BaseHTTPRequestHandler):
    """BaseHTTPRequestHandler, basically just here to have a configurable do_POST handler"""


last_submission = None
last_content_type = None
server_response = 'Hello, plugin'.encode('utf-8')


def server_accept_submit(handler):
    """Plugs into PostHandler.do_POST, accepts a POST on /submit and saves it into
    the globals"""

    global last_submission
    global last_content_type
    global server_response

    last_submission = None
    last_content_type = None

    if handler.path == "/submit":
        handler.send_response(200, 'Ok')
        content_length = int(handler.headers['Content-Length'])
        last_submission = handler.rfile.read(content_length)
        last_content_type = handler.headers['Content-Type']
    else:
        handler.send_response(404, 'Not found')
    handler.end_headers()

    handler.wfile.write(server_response)
    handler.wfile.flush()

    handler.log_message("sent response")


server_hang_event = None


def server_hang(handler):
    """Plugs into PostHandler.do_POST, waits on server_hang_event or until timeout"""
    server_hang_event.wait(SERVER_TIMEOUT_SECS)


def run_and_assert_result(handle_request, post_file, url, expected_result):
    """Sets up the server on another thread, runs the NSIS driver, and checks the result"""
    global last_submission
    global server_hang_event

    try:
        os.remove(RESULT_FILE_NAME)
    except FileNotFoundError:
        pass

    PostHandler.do_POST = handle_request
    last_submission = None

    def handler_thread():
        with socketserver.TCPServer((BIND_HOST, BIND_PORT), PostHandler) as httpd:
            httpd.timeout = SERVER_TIMEOUT_SECS
            httpd.handle_request()

    if handle_request:
        server_thread = threading.Thread(target=handler_thread)
        server_thread.start()

    try:
        subprocess.call([DRIVER_EXE_FILE_NAME, f'/postfile={post_file}', f'/url={url}',
                         f'/resultfile={RESULT_FILE_NAME}', '/S'], timeout=DRIVER_TIMEOUT_SECS)

        with open(RESULT_FILE_NAME, "r") as result_file:
            result = result_file.read()

            if result != expected_result:
                raise AssertionError(f'{result} != {expected_result}')

    finally:
        if server_hang_event:
            server_hang_event.set()

        if handle_request:
            server_thread.join()
        os.remove(RESULT_FILE_NAME)


def create_json_file(json_bytes=COMMON_JSON_BYTES):
    with open(JSON_FILE_NAME, "wb") as outfile:
        outfile.write(json_bytes)


def check_submission(json_bytes=COMMON_JSON_BYTES):
    if last_submission != json_bytes:
        raise AssertionError(f'{last_submission.hex()} != {COMMON_JSON_BYTES}')


def cleanup_json_file():
    os.remove(JSON_FILE_NAME)


# Tests begin here

try:
    cleanup_json_file()
except FileNotFoundError:
    pass

# Basic test

create_json_file()
run_and_assert_result(server_accept_submit, JSON_FILE_NAME, COMMON_URL, "success")
check_submission()
assert last_content_type == 'application/json'
cleanup_json_file()

print("Basic test OK\n")

# Test with missing file

try:
    cleanup_json_file()
except FileNotFoundError:
    pass

run_and_assert_result(None, JSON_FILE_NAME, COMMON_URL, "error reading file")

print("Missing file test OK\n")

# Test with empty file

create_json_file(bytes())
run_and_assert_result(server_accept_submit, JSON_FILE_NAME, COMMON_URL, "success")
check_submission(bytes())
cleanup_json_file()

print("Empty file test OK\n")

# Test with large file

# NOTE: Not actually JSON, but nothing here should care
four_mbytes = bytes([x & 255 for x in range(4*1024*1024)])
create_json_file(four_mbytes)
run_and_assert_result(server_accept_submit, JSON_FILE_NAME, COMMON_URL, "success")
if last_submission != four_mbytes:
    raise AssertionError("large file mismatch")
cleanup_json_file()

print("Large file test OK\n")

# Test with long file name

# Test with bad URL

bogus_url = "notAUrl"
create_json_file()
run_and_assert_result(None, JSON_FILE_NAME, bogus_url, "error parsing URL")
cleanup_json_file()

print("Bad URL test OK\n")

# Test with empty response

server_response = bytes()
create_json_file()
run_and_assert_result(server_accept_submit, JSON_FILE_NAME, COMMON_URL, "success")
check_submission()
cleanup_json_file()

print("Empty response test OK\n")

# Test with large response

server_response = four_mbytes
create_json_file()
run_and_assert_result(server_accept_submit, JSON_FILE_NAME, COMMON_URL, "success")
check_submission()
cleanup_json_file()

print("Large response test OK\n")

# Test with 404
# NOTE: This succeeds since the client doesn't check the status code

create_json_file()
nonexistent_url = f"http://{BIND_HOST}:{BIND_PORT}/bad"
run_and_assert_result(server_accept_submit, JSON_FILE_NAME, nonexistent_url, "success")
cleanup_json_file()

print("404 response test OK\n")

# Test with no server on the port
# NOTE: I'm assuming nothing else has been able to bind to the port

print("Running no server test, this will take a few seconds...")

create_json_file()
run_and_assert_result(None, JSON_FILE_NAME, COMMON_URL, "error sending HTTP request")
cleanup_json_file()

print("No server test OK\n")

# Test with server that hangs on response
# NOTE: HttpPostFile doesn't currently set the timeouts. Defaults seem to be around 30 seconds,
# but if they end up being longer than the 60 second driver timeout then this will fail.

print("Running server hang test, this will take up to a minute...")

server_hang_event = threading.Event()
create_json_file()
run_and_assert_result(server_hang, JSON_FILE_NAME, COMMON_URL, "error sending HTTP request")
cleanup_json_file()
server_hang_event = None

print("Server hang test OK\n")
