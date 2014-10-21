 # -*- coding: utf-8 -*-
import argparse
import json
import logging
import os
import signal
import socket
import sys
import threading
import time
import traceback
import urllib2
import uuid
from collections import defaultdict
from multiprocessing import Process, Event

repo_root = os.path.abspath(os.path.split(__file__)[0])

sys.path.insert(1, os.path.join(repo_root, "tools", "wptserve"))
from wptserve import server as wptserve, handlers
from wptserve.router import any_method
sys.path.insert(1, os.path.join(repo_root, "tools", "pywebsocket", "src"))
from mod_pywebsocket import standalone as pywebsocket

routes = [("GET", "/tools/runner/*", handlers.file_handler),
          ("POST", "/tools/runner/update_manifest.py", handlers.python_script_handler),
          (any_method, "/tools/*", handlers.ErrorHandler(404)),
          (any_method, "/serve.py", handlers.ErrorHandler(404)),
          (any_method, "*.py", handlers.python_script_handler),
          ("GET", "*.asis", handlers.as_is_handler),
          ("GET", "*", handlers.file_handler),
          ]

rewrites = [("GET", "/resources/WebIDLParser.js", "/resources/webidl2/lib/webidl2.js")]

subdomains = [u"www",
              u"www1",
              u"www2",
              u"天気の良い日",
              u"élève"]

logger = None

def default_logger(level):
    logger = logging.getLogger("web-platform-tests")
    logging.basicConfig(level=getattr(logging, level.upper()))
    return logger

def open_socket(port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    if port != 0:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('127.0.0.1', port))
    sock.listen(5)
    return sock

def get_port():
    free_socket = open_socket(0)
    port = free_socket.getsockname()[1]
    logger.debug("Going to use port %s" % port)
    free_socket.close()
    return port


class ServerProc(object):
    def __init__(self):
        self.proc = None
        self.daemon = None
        self.stop = Event()

    def start(self, init_func, host, port, paths, bind_hostname, external_config):
        self.proc = Process(target=self.create_daemon,
                            args=(init_func, host, port, paths, bind_hostname, external_config))
        self.proc.daemon = True
        self.proc.start()

    def create_daemon(self, init_func, host, port, paths, bind_hostname, external_config):
        try:
            self.daemon = init_func(host, port, paths, bind_hostname, external_config)
        except socket.error:
            print >> sys.stderr, "Socket error on port %s" % port
            raise
        except:
            print >> sys.stderr, traceback.format_exc()
            raise

        if self.daemon:
            self.daemon.start(block=False)
            try:
                self.stop.wait()
            except KeyboardInterrupt:
                pass

    def wait(self):
        self.stop.set()
        self.proc.join()

    def kill(self):
        self.stop.set()
        self.proc.terminate()
        self.proc.join()

    def is_alive(self):
        return self.proc.is_alive()

def check_subdomains(host, paths, bind_hostname):
    port = get_port()
    subdomains = get_subdomains(host)

    wrapper = ServerProc()
    wrapper.start(start_http_server, host, port, paths, bind_hostname, None)

    connected = False
    for i in range(10):
        try:
            urllib2.urlopen("http://%s:%d/" % (host, port))
            connected = True
            break
        except urllib2.URLError:
            time.sleep(1)

    if not connected:
        logger.critical("Failed to connect to test server on http://%s:%s You may need to edit /etc/hosts or similar" % (host, port))
        sys.exit(1)

    for subdomain, (punycode, host) in subdomains.iteritems():
        domain = "%s.%s" % (punycode, host)
        try:
            urllib2.urlopen("http://%s:%d/" % (domain, port))
        except Exception as e:
            logger.critical("Failed probing domain %s. You may need to edit /etc/hosts or similar." % domain)
            sys.exit(1)

    wrapper.wait()

def get_subdomains(host):
    #This assumes that the tld is ascii-only or already in punycode
    return {subdomain: (subdomain.encode("idna"), host)
            for subdomain in subdomains}

def start_servers(host, ports, paths, bind_hostname, external_config):
    servers = defaultdict(list)

    for scheme, ports in ports.iteritems():
        assert len(ports) == {"http":2}.get(scheme, 1)

        for port  in ports:
            init_func = {"http":start_http_server,
                         "https":start_https_server,
                         "ws":start_ws_server,
                         "wss":start_wss_server}[scheme]

            server_proc = ServerProc()
            server_proc.start(init_func, host, port, paths, bind_hostname, external_config)
            servers[scheme].append((port, server_proc))

    return servers

def start_http_server(host, port, paths, bind_hostname, external_config):
    return wptserve.WebTestHttpd(host=host,
                                 port=port,
                                 doc_root=paths["doc_root"],
                                 routes=routes,
                                 rewrites=rewrites,
                                 bind_hostname=bind_hostname,
                                 config=external_config,
                                 use_ssl=False,
                                 certificate=None)

def start_https_server(host, port, paths, bind_hostname):
    return

class WebSocketDaemon(object):
    def __init__(self, host, port, doc_root, handlers_root, log_level, bind_hostname):
        self.host = host
        cmd_args = ["-p", port,
                    "-d", doc_root,
                    "-w", handlers_root,
                    "--log-level", log_level]
        if (bind_hostname):
            cmd_args = ["-H", host] + cmd_args
        opts, args = pywebsocket._parse_args_and_config(cmd_args)
        opts.cgi_directories = []
        opts.is_executable_method = None
        self.server = pywebsocket.WebSocketServer(opts)
        ports = [item[0].getsockname()[1] for item in self.server._sockets]
        assert all(item == ports[0] for item in ports)
        self.port = ports[0]
        self.started = False
        self.server_thread = None

    def start(self, block=False):
        self.started = True
        if block:
            self.server.serve_forever()
        else:
            self.server_thread = threading.Thread(target=self.server.serve_forever)
            self.server_thread.setDaemon(True)  # don't hang on exit
            self.server_thread.start()

    def stop(self):
        """
        Stops the server.

        If the server is not running, this method has no effect.
        """
        if self.started:
            try:
                self.server.shutdown()
                self.server.server_close()
                self.server_thread.join()
                self.server_thread = None
            except AttributeError:
                pass
            self.started = False
        self.server = None

def start_ws_server(host, port, paths, bind_hostname, external_config):
    return WebSocketDaemon(host,
                           str(port),
                           repo_root,
                           paths["ws_doc_root"],
                           "debug",
                           bind_hostname)

def start_wss_server(host, port, path, bind_hostname, external_config):
    return

def get_ports(config):
    rv = defaultdict(list)
    for scheme, ports in config["ports"].iteritems():
        for i, port in enumerate(ports):
            if port == "auto":
                port = get_port()
            else:
                port = port
            rv[scheme].append(port)
    return rv

def normalise_config(config, ports):
    host = config["external_host"] if config["external_host"] else config["host"]
    domains = get_subdomains(host)

    for key, value in domains.iteritems():
        domains[key] = ".".join(value)

    domains[""] = host

    ports_ = {}
    for scheme, ports_used in ports.iteritems():
        ports_[scheme] = ports_used

    return {"host": host,
            "domains": domains,
            "ports": ports_}

def start(config):
    host = config["host"]
    domains = get_subdomains(host)
    ports = get_ports(config)
    bind_hostname = config["bind_hostname"]

    paths = {"doc_root": config["doc_root"],
             "ws_doc_root": config["ws_doc_root"]}

    if config["check_subdomains"]:
        check_subdomains(host, paths, bind_hostname)

    external_config = normalise_config(config, ports)

    servers = start_servers(host, ports, paths, bind_hostname, external_config)

    return external_config, servers


def iter_procs(servers):
    for servers in servers.values():
        for port, server in servers:
            yield server.proc

def value_set(config, key):
    return key in config and config[key] is not None

def set_computed_defaults(config):
    if not value_set(config, "ws_doc_root"):
        if value_set(config, "doc_root"):
            root = config["doc_root"]
        else:
            root = repo_root
        config["ws_doc_root"] = os.path.join(repo_root, "websockets", "handlers")

    if not value_set(config, "doc_root"):
        config["doc_root"] = repo_root


def merge_json(base_obj, override_obj):
    rv = {}
    for key, value in base_obj.iteritems():
        if key not in override_obj:
            rv[key] = value
        else:
            if isinstance(value, dict):
                rv[key] = merge_json(value, override_obj[key])
            else:
                rv[key] = override_obj[key]
    return rv

def load_config(default_path, override_path=None):
    if os.path.exists(default_path):
        with open(default_path) as f:
            base_obj = json.load(f)
    else:
        raise ValueError("Config path %s does not exist" % default_path)

    if os.path.exists(override_path):
        with open(override_path) as f:
            override_obj = json.load(f)
    else:
        override_obj = {}
    rv = merge_json(base_obj, override_obj)

    set_computed_defaults(rv)
    return rv

def main():
    global logger

    config = load_config("config.default.json",
                         "config.json")

    logger = default_logger(config["log_level"])

    config_, servers = start(config)

    try:
        while any(item.is_alive() for item in iter_procs(servers)):
            for item in iter_procs(servers):
                item.join(1)
    except KeyboardInterrupt:
        logger.info("Shutting down")

if __name__ == "__main__":
    main()
