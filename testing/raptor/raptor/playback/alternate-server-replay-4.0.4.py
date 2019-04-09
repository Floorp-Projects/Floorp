# This file was copied from  mitmproxy/mitmproxy/addons/serverplayback.py release tag 4.0.4
# and modified by Florin Strugariu

# Altered features:
# * returns 404 rather than dropping the whole HTTP/2 connection on the floor
# * remove the replay packages that don't have any content in their response package

import hashlib
import urllib

import typing
from urllib import parse
from mitmproxy import command
from mitmproxy import ctx, http
from mitmproxy import exceptions
from mitmproxy import io


class AlternateServerPlayback:

    def __init__(self):
        ctx.master.addons.remove(ctx.master.addons.get("serverplayback"))
        self.flowmap = {}
        self.configured = False

    def load(self, loader):
        ctx.log.info("load options")
        loader.add_option(
            "server_replay", typing.Sequence[str], [],
            "Replay server responses from a saved file."
        )

    @command.command("replay.server")
    def load_flows(self, flows):
        """
            Replay server responses from flows.
        """
        self.flowmap = {}
        for i in flows:
            # Check that response has data.content. If response has no content a
            # HttpException("Cannot assemble flow with missing content") will get raised
            if i.response and i.response.data.content:
                l = self.flowmap.setdefault(self._hash(i), [])
                l.append(i)
            else:
                ctx.log.info(
                    "Request %s has no response data content. Removing from request list" %
                    i.request.url)
        ctx.master.addons.trigger("update", [])

    @command.command("replay.server.file")
    def load_file(self, path):
        try:
            flows = io.read_flows_from_paths([path])
        except exceptions.FlowReadException as e:
            raise exceptions.CommandError(str(e))
        self.load_flows(flows)

    @command.command("replay.server.stop")
    def clear(self):
        """
            Stop server replay.
        """
        self.flowmap = {}
        ctx.master.addons.trigger("update", [])

    @command.command("replay.server.count")
    def count(self):
        return sum([len(i) for i in self.flowmap.values()])

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

        return hashlib.sha256(
            repr(key).encode("utf8", "surrogateescape")
        ).digest()

    def next_flow(self, request):
        """
            Returns the next flow object, or None if no matching flow was
            found.
        """
        hsh = self._hash(request)
        if hsh in self.flowmap:
            return self.flowmap[hsh][-1]

    def configure(self, updated):
        if not self.configured and ctx.options.server_replay:
            self.configured = True
            try:
                flows = io.read_flows_from_paths(ctx.options.server_replay)
            except exceptions.FlowReadException as e:
                raise exceptions.OptionsError(str(e))
            self.load_flows(flows)

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
                f.response = http.HTTPResponse.make(404, b'', {'content-type': 'text/plain'})


addons = [AlternateServerPlayback()]
