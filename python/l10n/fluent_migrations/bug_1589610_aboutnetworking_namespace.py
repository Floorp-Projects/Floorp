# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1589610 - Make about:networking fluent IDs have a specific namespace, part {index}"""

    ctx.add_transforms(
        'toolkit/toolkit/about/aboutNetworking.ftl',
        'toolkit/toolkit/about/aboutNetworking.ftl',
        transforms_from(
            """

about-networking-http = {COPY_PATTERN(from_path, "http")}

about-networking-title = {COPY_PATTERN(from_path, "title")}

about-networking-sockets = {COPY_PATTERN(from_path, "sockets")}

about-networking-dns = {COPY_PATTERN(from_path, "dns")}

about-networking-dns-suffix = {COPY_PATTERN(from_path, "dnssuffix")}

about-networking-websockets = {COPY_PATTERN(from_path, "websockets")}

about-networking-refresh = {COPY_PATTERN(from_path, "refresh")}

about-networking-auto-refresh = {COPY_PATTERN(from_path, "auto-refresh")}

about-networking-hostname = {COPY_PATTERN(from_path, "hostname")}

about-networking-port = {COPY_PATTERN(from_path, "port")}

about-networking-http-version = {COPY_PATTERN(from_path, "http-version")}

about-networking-ssl = {COPY_PATTERN(from_path, "ssl")}

about-networking-active = {COPY_PATTERN(from_path, "active")}

about-networking-idle = {COPY_PATTERN(from_path, "idle")}

about-networking-host = {COPY_PATTERN(from_path, "host")}

about-networking-tcp = {COPY_PATTERN(from_path, "tcp")}

about-networking-sent = {COPY_PATTERN(from_path, "sent")}

about-networking-received = {COPY_PATTERN(from_path, "received")}

about-networking-family = {COPY_PATTERN(from_path, "family")}

about-networking-trr = {COPY_PATTERN(from_path, "trr")}

about-networking-addresses = {COPY_PATTERN(from_path, "addresses")}

about-networking-expires = {COPY_PATTERN(from_path, "expires")}

about-networking-messages-sent = {COPY_PATTERN(from_path, "messages-sent")}

about-networking-messages-received = {COPY_PATTERN(from_path, "messages-received")}

about-networking-bytes-sent = {COPY_PATTERN(from_path, "bytes-sent")}

about-networking-bytes-received = {COPY_PATTERN(from_path, "bytes-received")}

about-networking-logging = {COPY_PATTERN(from_path, "logging")}

about-networking-log-tutorial = {COPY_PATTERN(from_path, "log-tutorial")}

about-networking-current-log-file = {COPY_PATTERN(from_path, "current-log-file")}

about-networking-current-log-modules = {COPY_PATTERN(from_path, "current-log-modules")}

about-networking-set-log-file = {COPY_PATTERN(from_path, "set-log-file")}

about-networking-set-log-modules = {COPY_PATTERN(from_path, "set-log-modules")}

about-networking-start-logging = {COPY_PATTERN(from_path, "start-logging")}

about-networking-stop-logging = {COPY_PATTERN(from_path, "stop-logging")}

about-networking-dns-lookup = {COPY_PATTERN(from_path, "dns-lookup")}

about-networking-dns-lookup-button = {COPY_PATTERN(from_path, "dns-lookup-button")}

about-networking-dns-domain = {COPY_PATTERN(from_path, "dns-domain")}

about-networking-dns-lookup-table-column = {COPY_PATTERN(from_path, "dns-lookup-table-column")}

about-networking-rcwn = {COPY_PATTERN(from_path, "rcwn")}

about-networking-rcwn-status = {COPY_PATTERN(from_path, "rcwn-status")}

about-networking-rcwn-cache-won-count = {COPY_PATTERN(from_path, "rcwn-cache-won-count")}

about-networking-rcwn-net-won-count = {COPY_PATTERN(from_path, "rcwn-net-won-count")}

about-networking-total-network-requests = {COPY_PATTERN(from_path, "total-network-requests")}

about-networking-rcwn-operation = {COPY_PATTERN(from_path, "rcwn-operation")}

about-networking-rcwn-perf-open = {COPY_PATTERN(from_path, "rcwn-perf-open")}

about-networking-rcwn-perf-read = {COPY_PATTERN(from_path, "rcwn-perf-read")}

about-networking-rcwn-perf-write = {COPY_PATTERN(from_path, "rcwn-perf-write")}

about-networking-rcwn-perf-entry-open = {COPY_PATTERN(from_path, "rcwn-perf-entry-open")}

about-networking-rcwn-avg-short = {COPY_PATTERN(from_path, "rcwn-avg-short")}

about-networking-rcwn-avg-long = {COPY_PATTERN(from_path, "rcwn-avg-long")}

about-networking-rcwn-std-dev-long = {COPY_PATTERN(from_path, "rcwn-std-dev-long")}

about-networking-rcwn-cache-slow = {COPY_PATTERN(from_path, "rcwn-cache-slow")}

about-networking-rcwn-cache-not-slow = {COPY_PATTERN(from_path, "rcwn-cache-not-slow")}

about-networking-networkid = {COPY_PATTERN(from_path, "networkid")}

about-networking-networkid-id = {COPY_PATTERN(from_path, "networkid-id")}

about-networking-networkid-is-up = {COPY_PATTERN(from_path, "networkid-isUp")}

about-networking-networkid-status-known = {COPY_PATTERN(from_path, "networkid-statusKnown")}

""", from_path="toolkit/toolkit/about/aboutNetworking.ftl"),
        )
