import time

from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
    # # must set request.ws_protocol to the selected version from ws_requested_protocols
    for x in request.ws_requested_protocols:
        if x != "test-does-not-exist":
            request.ws_protocol = x
            break

    if request.ws_protocol == "test-3":
        time.sleep(3)
    elif request.ws_protocol == "test-6":
        time.sleep(6)
    else:
        pass


def web_socket_passive_closing_handshake(request):
    if request.ws_close_code == 1005:
        return None, None
    return request.ws_close_code, request.ws_close_reason


def web_socket_transfer_data(request):
    msgutil.close_connection(request)
