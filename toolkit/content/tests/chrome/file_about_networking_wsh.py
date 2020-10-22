from mod_pywebsocket import msgutil

def web_socket_do_extra_handshake(request):
	pass

def web_socket_transfer_data(request):
	while not request.client_terminated:
		msgutil.receive_message(request)

