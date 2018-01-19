from __future__ import absolute_import

import os
import platform
import socket
import subprocess
import time

from .client import Client


class Server(object):

    def __init__(self, path='browsermob-proxy', options={}):
        """
        Initialises a Server object

        :param path: Path to the browsermob proxy batch file
        :param options: Dictionary that can hold the port. \
                     More items will be added in the future. \
                     This defaults to an empty dictionary
        """
        path_var_sep = ':'
        if platform.system() == 'Windows':
            path_var_sep = ';'
            if not path.endswith('.bat'):
                path += '.bat'

        exec_not_on_path = True
        for directory in os.environ['PATH'].split(path_var_sep):
            if(os.path.isfile(os.path.join(directory, path))):
                exec_not_on_path = False
                break

        if not os.path.isfile(path) and exec_not_on_path:
            raise IOError("Browsermob-Proxy binary couldn't be found in path"
                          " provided: {}".format(path))

        self.path = path
        self.port = options.get('port', 8080)
        self.process = None

        if platform.system() == 'Darwin':
            self.command = ['sh']
        else:
            self.command = []
        self.command += [path, '--port={}'.format(self.port)]

    def start(self):
        """
        This will start the browsermob proxy and then wait until it can
        interact with it
        """
        self.log_file = open(os.path.abspath('server.log'), 'w')
        self.process = subprocess.Popen(self.command,
                                        stdout=self.log_file,
                                        stderr=subprocess.STDOUT)
        count = 0
        while not self._is_listening():
            time.sleep(0.5)
            count += 1
            if count == 60:
                self.stop()
                raise Exception("Can't connect to Browsermob-Proxy")

    def stop(self):
        """
        This will stop the process running the proxy
        """
        if self.process.poll() is not None:
            return

        try:
            self.process.kill()
            self.process.wait()
        except AttributeError:
            # kill may not be available under windows environment
            pass

        self.log_file.close()

    @property
    def url(self):
        """
        Gets the url that the proxy is running on. This is not the URL clients
        should connect to.
        """
        return "http://localhost:{}".format(self.port)

    def create_proxy(self, params={}):
        """
        Gets a client class that allow to set all the proxy details that you
        may need to.
        :param params: Dictionary where you can specify params \
                    like httpProxy and httpsProxy
        """
        client = Client(self.url[7:], params)
        return client

    def _is_listening(self):
        try:
            socket_ = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            socket_.settimeout(1)
            socket_.connect(("localhost", self.port))
            socket_.close()
            return True
        except socket.error:
            return False
