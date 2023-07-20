# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file was copied from
# https://github.com/mitmproxy/mitmproxy/blob/v7.0.4/mitmproxy/addons/serverplayback.py
# and modified by Kimberly Sereduck

# Altered features:
# * request function returns 404 rather than killing the flow which results in test failure
# * added option to play flows in reverse order: alt_server_replay_order_reversed
# * TO DO: remove the replay packages that don't have any content in their response package,
# see Bug 1739418: https://bugzilla.mozilla.org/show_bug.cgi?id=1739418

import hashlib
import traceback
import urllib
from collections.abc import Hashable, Sequence
from typing import Any, Optional

import mitmproxy.types
from mitmproxy import command, ctx, exceptions, flow, hooks, http, io


class AltServerPlayback:
    flowmap: dict[Hashable, list[http.HTTPFlow]]
    configured: bool

    def __init__(self):
        self.flowmap = {}
        self.configured = False

    def load(self, loader):
        loader.add_option(
            "alt_server_replay_kill_extra",
            bool,
            False,
            "Kill extra requests during replay.",
        )
        loader.add_option(
            "alt_server_replay_nopop",
            bool,
            False,
            """
            Don't remove flows from server replay state after use. This makes it
            possible to replay same response multiple times.
            """,
        )
        loader.add_option(
            "alt_server_replay_refresh",
            bool,
            True,
            """
            Refresh server replay responses by adjusting date, expires and
            last-modified headers, as well as adjusting cookie expiration.
            """,
        )
        loader.add_option(
            "alt_server_replay_use_headers",
            Sequence[str],
            [],
            "Request headers to be considered during replay.",
        )
        loader.add_option(
            "alt_server_replay",
            Sequence[str],
            [],
            "Replay server responses from a saved file.",
        )
        loader.add_option(
            "alt_server_replay_ignore_content",
            bool,
            False,
            "Ignore request's content while searching for a saved flow to replay.",
        )
        loader.add_option(
            "alt_server_replay_ignore_params",
            Sequence[str],
            [],
            """
            Request's parameters to be ignored while searching for a saved flow
            to replay.
            """,
        )
        loader.add_option(
            "alt_server_replay_ignore_payload_params",
            Sequence[str],
            [],
            """
            Request's payload parameters (application/x-www-form-urlencoded or
            multipart/form-data) to be ignored while searching for a saved flow
            to replay.
            """,
        )
        loader.add_option(
            "alt_server_replay_ignore_host",
            bool,
            False,
            """
            Ignore request's destination host while searching for a saved flow
            to replay.
            """,
        )
        loader.add_option(
            "alt_server_replay_ignore_port",
            bool,
            False,
            """
            Ignore request's destination port while searching for a saved flow
            to replay.
            """,
        )
        loader.add_option(
            "alt_server_replay_order_reversed",
            bool,
            False,
            """
            Reverse the order of flows when replaying.
            """,
        )

    @command.command("replay.server")
    def load_flows(self, flows: Sequence[flow.Flow]) -> None:
        """
        Replay server responses from flows.
        """
        self.flowmap = {}
        if ctx.options.alt_server_replay_order_reversed:
            flows.reverse()
        for f in flows:
            if isinstance(f, http.HTTPFlow):
                lst = self.flowmap.setdefault(self._hash(f), [])
                lst.append(f)
        ctx.master.addons.trigger(hooks.UpdateHook([]))

    @command.command("replay.server.file")
    def load_file(self, path: mitmproxy.types.Path) -> None:
        try:
            flows = io.read_flows_from_paths([path])
        except exceptions.FlowReadException as e:
            raise exceptions.CommandError(str(e))
        self.load_flows(flows)

    @command.command("replay.server.stop")
    def clear(self) -> None:
        """
        Stop server replay.
        """
        self.flowmap = {}
        ctx.master.addons.trigger(hooks.UpdateHook([]))

    @command.command("replay.server.count")
    def count(self) -> int:
        return sum([len(i) for i in self.flowmap.values()])

    def _hash(self, flow: http.HTTPFlow) -> Hashable:
        """
        Calculates a loose hash of the flow request.
        """
        r = flow.request
        _, _, path, _, query, _ = urllib.parse.urlparse(r.url)
        queriesArray = urllib.parse.parse_qsl(query, keep_blank_values=True)

        key: list[Any] = [str(r.scheme), str(r.method), str(path)]
        if not ctx.options.alt_server_replay_ignore_content:
            if ctx.options.alt_server_replay_ignore_payload_params and r.multipart_form:
                key.extend(
                    (k, v)
                    for k, v in r.multipart_form.items(multi=True)
                    if k.decode(errors="replace")
                    not in ctx.options.alt_server_replay_ignore_payload_params
                )
            elif (
                ctx.options.alt_server_replay_ignore_payload_params
                and r.urlencoded_form
            ):
                key.extend(
                    (k, v)
                    for k, v in r.urlencoded_form.items(multi=True)
                    if k not in ctx.options.alt_server_replay_ignore_payload_params
                )
            else:
                key.append(str(r.raw_content))

        if not ctx.options.alt_server_replay_ignore_host:
            key.append(r.pretty_host)
        if not ctx.options.alt_server_replay_ignore_port:
            key.append(r.port)

        filtered = []
        ignore_params = ctx.options.alt_server_replay_ignore_params or []
        for p in queriesArray:
            if p[0] not in ignore_params:
                filtered.append(p)
        for p in filtered:
            key.append(p[0])
            key.append(p[1])

        if ctx.options.alt_server_replay_use_headers:
            headers = []
            for i in ctx.options.alt_server_replay_use_headers:
                v = r.headers.get(i)
                headers.append((i, v))
            key.append(headers)
        return hashlib.sha256(repr(key).encode("utf8", "surrogateescape")).digest()

    def next_flow(self, flow: http.HTTPFlow) -> Optional[http.HTTPFlow]:
        """
        Returns the next flow object, or None if no matching flow was
        found.
        """
        hash = self._hash(flow)
        if hash in self.flowmap:
            if ctx.options.alt_server_replay_nopop:
                return next(
                    (flow for flow in self.flowmap[hash] if flow.response), None
                )
            else:
                ret = self.flowmap[hash].pop(0)
                while not ret.response:
                    if self.flowmap[hash]:
                        ret = self.flowmap[hash].pop(0)
                    else:
                        del self.flowmap[hash]
                        return None
                if not self.flowmap[hash]:
                    del self.flowmap[hash]
                return ret
        else:
            return None

    def configure(self, updated):
        if not self.configured and ctx.options.alt_server_replay:
            self.configured = True
            try:
                flows = io.read_flows_from_paths(
                    ctx.options.alt_server_replay[0].split(",")
                )
            except exceptions.FlowReadException:
                raise exceptions.OptionsError(str(traceback.print_exc()))
            self.load_flows(flows)

    def request(self, f: http.HTTPFlow) -> None:
        if self.flowmap:
            rflow = self.next_flow(f)
            if rflow:
                assert rflow.response
                response = rflow.response.copy()
                if ctx.options.alt_server_replay_refresh:
                    response.refresh()
                f.response = response
                f.is_replay = "response"
            elif ctx.options.alt_server_replay_kill_extra:
                ctx.log.warn(
                    "server_playback: killed non-replay request {}".format(
                        f.request.url
                    )
                )
                f.response = http.Response.make(
                    404, b"", {"content-type": "text/plain"}
                )


addons = [AltServerPlayback()]
