import datetime
import os
import socket
import subprocess
import time


class B2GInstance(object):

    def __init__(self, host, port, b2gbin):
        self.marionette_host = host
        self.marionette_port = port
        self.b2gbin = b2gbin
        self.proc = None

    def start(self):
        if not os.getenv('GAIA'):
            raise Exception("GAIA environment variable must be set to your gaia directory")
        args = [self.b2gbin, '-profile', os.path.join(os.getenv('GAIA'), "profile")]
        self.proc = subprocess.Popen(args,
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT)

    def close(self):
        self.proc.terminate()

    def wait_for_port(self, timeout=300):
        assert(self.marionette_port)
        starttime = datetime.datetime.now()
        while datetime.datetime.now() - starttime < datetime.timedelta(seconds=timeout):
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.connect((self.marionette_host, self.marionette_port))
                data = sock.recv(16)
                sock.close()
                if '"from"' in data:
                    return True
            except:
                import traceback
                print traceback.format_exc()
            time.sleep(1)
        return False

