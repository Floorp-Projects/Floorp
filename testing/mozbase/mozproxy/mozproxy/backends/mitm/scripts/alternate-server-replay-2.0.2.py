# This file was copied from mitmproxy/addons/serverplayback.py release tag 2.0.2 and modified by
# Benjamin Smedberg

# Altered features:
# * --kill returns 404 rather than dropping the whole HTTP/2 connection on the floor
# * best-match response handling is used to improve success rates
from __future__ import absolute_import, print_function

import os
import hashlib
import sys
import urllib
from collections import defaultdict
import json

from mitmproxy import ctx
from mitmproxy import exceptions
from mitmproxy import http
from mitmproxy import io
from typing import Any  # noqa
from typing import List  # noqa

# PATCHING AREA  - ALLOWS HTTP/2 WITH NO CERT SNIFFING
from mitmproxy.proxy.protocol import tls
from mitmproxy.proxy.protocol.http2 import Http2Layer, SafeH2Connection

_PROTO = {}


@property
def _alpn(self):
    proto = _PROTO.get(self.server_sni)
    if proto is None:
        return self.server_conn.get_alpn_proto_negotiated()
    if proto.startswith(b"HTTP/2"):
        return b"h2"
    elif proto.startswith(b"HTTP/1"):
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


class ServerPlayback:
    def __init__(self, replayfiles):
        if len(replayfiles) > 0:
            for path in replayfiles:
                proto = os.path.splitext(path)[0] + ".json"
                if os.path.exists(proto):
                    ctx.log.info("Loading proto info from %s" % proto)
                    with open(proto) as f:
                        p = json.loads(f.read()).get('http_protocol')

                        if p is not None:
                            _PROTO.update(p)

        self.options = None
        self.replayfiles = replayfiles
        self.flowmap = {}

    def load(self, flows):
        for i in flows:
            if i.response:
                l = self.flowmap.setdefault(self._hash(i.request), [])
                l.append(i)

    def clear(self):
        self.flowmap = {}

    def _parse(self, r):
        """
            Return (path, queries, formdata, content) for a request.
        """
        _, _, path, _, query, _ = urllib.parse.urlparse(r.url)
        queriesArray = urllib.parse.parse_qsl(query, keep_blank_values=True)
        queries = defaultdict(list)
        for k, v in queriesArray:
            queries[k].append(v)

        content = None
        formdata = None
        if r.raw_content != b"":
            if r.multipart_form:
                formdata = r.multipart_form
            elif r.urlencoded_form:
                formdata = r.urlencoded_form
            else:
                content = r.content
        return (path, queries, formdata, content)

    def _hash(self, r):
        """
            Calculates a loose hash of the flow request.
        """
        path, queries, _, _ = self._parse(r)

        key = [str(r.port), str(r.scheme), str(r.method), str(path)]  # type: List[Any]
        if not self.options.server_replay_ignore_host:
            key.append(r.host)

        if len(queries):
            key.append("?")

        return hashlib.sha256(repr(key).encode("utf8", "surrogateescape")).digest()

    def _match(self, request_a, request_b):
        """
            Calculate a match score between two requests.
            Match algorithm:
              * identical query keys: 3 points
              * matching query param present: 1 point
              * matching query param value: 3 points
              * identical form keys: 3 points
              * matching form param present: 1 point
              * matching form param value: 3 points
              * matching body (no multipart or encoded form): 4 points
        """
        match = 0

        path_a, queries_a, form_a, content_a = self._parse(request_a)
        path_b, queries_b, form_b, content_b = self._parse(request_b)

        keys_a = set(queries_a.keys())
        keys_b = set(queries_b.keys())
        if keys_a == keys_b:
            match += 3

        for key in keys_a:
            values_a = set(queries_a[key])
            values_b = set(queries_b[key])
            if len(values_a) == len(values_b):
                match += 1
            if values_a == values_b:
                match += 3

        if form_a and form_b:
            keys_a = set(form_a.keys())
            keys_b = set(form_b.keys())
            if keys_a == keys_b:
                match += 3

            for key in keys_a:
                values_a = set(form_a.get_all(key))
                values_b = set(form_b.get_all(key))
                if len(values_a) == len(values_b):
                    match += 1
                if values_a == values_b:
                    match += 3

        elif content_a and (content_a == content_b):
            match += 4

        return match

    def next_flow(self, request):
        """
            Returns the next flow object, or None if no matching flow was
            found.
        """
        hsh = self._hash(request)
        flows = self.flowmap.get(hsh, None)
        if flows is None:
            return None

        # if it's an exact match, great!
        if len(flows) == 1:
            candidate = flows[0]
            if (
                candidate.request.url == request.url
                and candidate.request.raw_content == request.raw_content
            ):
                ctx.log.info(
                    "For request {} found exact replay match".format(request.url)
                )
                return candidate

        # find the best match between the request and the available flow candidates
        match = -1
        flow = None
        ctx.log.debug("Candidate flows for request: {}".format(request.url))
        for candidate_flow in flows:
            candidate_match = self._match(candidate_flow.request, request)
            ctx.log.debug(
                "  score={} url={}".format(candidate_match, candidate_flow.request.url)
            )
            if candidate_match >= match:
                match = candidate_match
                flow = candidate_flow
        ctx.log.info(
            "For request {} best match {} with score=={}".format(
                request.url, flow.request.url, match
            )
        )
        return flow

    def configure(self, options, updated):
        self.options = options
        self.clear()
        try:
            flows = io.read_flows_from_paths(self.replayfiles)
        except exceptions.FlowReadException as e:
            raise exceptions.OptionsError(str(e))
        self.load(flows)

    def request(self, f):
        if self.flowmap:
            rflow = self.next_flow(f.request)
            if rflow:
                response = rflow.response.copy()
                response.is_replay = True
                if self.options.refresh_server_playback:
                    response.refresh()
                f.response = response
            elif self.options.replay_kill_extra:
                ctx.log.warn(
                    "server_playback: killed non-replay request {}".format(
                        f.request.url
                    )
                )
                f.response = http.HTTPResponse.make(
                    404, b"", {"content-type": "text/plain"}
                )


def start():
    files = sys.argv[1:]
    print("Replaying from files: {}".format(files))
    return ServerPlayback(files)
