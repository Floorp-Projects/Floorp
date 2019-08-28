# This file was copied from  mitmproxy/mitmproxy/addons/serverplayback.py release tag 4.0.4
# and modified by Florin Strugariu

# Altered features:
# * returns 404 rather than dropping the whole HTTP/2 connection on the floor
# * remove the replay packages that don't have any content in their response package
from __future__ import absolute_import, print_function

import os
import json
import hashlib
import urllib

import typing
from urllib import parse

from mitmproxy import ctx, http
from mitmproxy import exceptions
from mitmproxy import io

# PATCHING AREA  - ALLOWS HTTP/2 WITH NO CERT SNIFFING
from mitmproxy.proxy.protocol import tls
from mitmproxy.proxy.protocol.http2 import Http2Layer, SafeH2Connection

_PROTO = {}


@property
def _alpn(self):
    proto = _PROTO.get(self.server_sni)
    if proto is None:
        return self.server_conn.get_alpn_proto_negotiated()
    if proto.startswith("HTTP/2"):
        return b"h2"
    elif proto.startswith("HTTP/1"):
        return b"h1"
    return b""


tls.TlsLayer.alpn_for_client_connection = _alpn


def _server_conn(self):
    if not self.server_conn.connected() and self.server_conn not in self.connections:
        # we can't use ctx.log in this layer
        print("Ignored CONNECT call on upstream server")
        return
    if self.server_conn.connected():
        import h2.config

        config = h2.config.H2Configuration(
            client_side=True,
            header_encoding=False,
            validate_outbound_headers=False,
            validate_inbound_headers=False,
        )
        self.connections[self.server_conn] = SafeH2Connection(
            self.server_conn, config=config
        )
    self.connections[self.server_conn].initiate_connection()
    self.server_conn.send(self.connections[self.server_conn].data_to_send())


Http2Layer._initiate_server_conn = _server_conn


def _remote_settings_changed(self, event, other_conn):
    if other_conn not in self.connections:
        # we can't use ctx.log in this layer
        print("Ignored remote settings upstream")
        return True
    new_settings = dict(
        [(key, cs.new_value) for (key, cs) in event.changed_settings.items()]
    )
    self.connections[other_conn].safe_update_settings(new_settings)
    return True


Http2Layer._handle_remote_settings_changed = _remote_settings_changed
# END OF PATCHING


class AlternateServerPlayback:
    def __init__(self):
        ctx.master.addons.remove(ctx.master.addons.get("serverplayback"))
        self.flowmap = {}
        self.configured = False

    def load(self, loader):
        loader.add_option(
            "server_replay_files",
            typing.Sequence[str],
            [],
            "Replay server responses from a saved file.",
        )

    def load_flows(self, flows):
        """
            Replay server responses from flows.
        """
        for i in flows:
            if i.type == 'websocket':
                ctx.log.info(
                    "Request is a WebSocketFlow. Removing from request list as WebSockets"
                    " are dissabled "
                )
            elif i.response:
                l = self.flowmap.setdefault(self._hash(i), [])
                l.append(i)
            else:
                ctx.log.info(
                    "Request %s has no response. Removing from request list"
                    % i.request.url
                )
        ctx.master.addons.trigger("update", [])

    def load_files(self, paths):
        if "," in paths[0]:
            paths = paths[0].split(",")
        for path in paths:
            ctx.log.info("Loading flows from %s" % path)
            try:
                flows = io.read_flows_from_paths([path])
            except exceptions.FlowReadException as e:
                raise exceptions.CommandError(str(e))
            self.load_flows(flows)
            proto = os.path.splitext(path)[0] + ".json"
            if os.path.exists(proto):
                ctx.log.info("Loading proto info from %s" % proto)
                with open(proto) as f:
                    recording_info = json.loads(f.read())
                ctx.log.info(
                    "Replaying file {} recorded on {}".format(
                        os.path.basename(path), recording_info["recording_date"]
                    )
                )
                _PROTO.update(recording_info["http_protocol"])

    def _hash(self, flow):
        """
            Calculates a loose hash of the flow request.
        """
        r = flow.request

        # unquote url
        # See Bug 1509835
        _, _, path, _, query, _ = urllib.parse.urlparse(parse.unquote(r.url))
        queriesArray = urllib.parse.parse_qsl(query, keep_blank_values=True)

        key = [str(r.port), str(r.scheme), str(r.method), str(path)]
        key.append(str(r.raw_content))
        key.append(r.host)

        for p in queriesArray:
            key.append(p[0])
            key.append(p[1])

        return hashlib.sha256(repr(key).encode("utf8", "surrogateescape")).digest()

    def next_flow(self, request):
        """
            Returns the next flow object, or None if no matching flow was
            found.
        """
        hsh = self._hash(request)
        if hsh in self.flowmap:
            return self.flowmap[hsh][-1]

    def configure(self, updated):
        if not self.configured and ctx.options.server_replay_files:
            self.configured = True
            self.load_files(ctx.options.server_replay_files)

    def request(self, f):
        if self.flowmap:
            rflow = self.next_flow(f)
            if rflow:
                response = rflow.response.copy()
                response.is_replay = True
                # Refresh server replay responses by adjusting date, expires and
                # last-modified headers, as well as adjusting cookie expiration.
                response.refresh()

                f.response = response
            else:
                # returns 404 rather than dropping the whole HTTP/2 connection
                ctx.log.warn(
                    "server_playback: killed non-replay request {}".format(
                        f.request.url
                    )
                )
                f.response = http.HTTPResponse.make(
                    404, b"", {"content-type": "text/plain"}
                )


addons = [AlternateServerPlayback()]
