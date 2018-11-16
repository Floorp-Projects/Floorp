from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import CONCAT
from fluent.migrate import REPLACE
from fluent.migrate import COPY


def migrate(ctx):
    """ Bug 1504751 - Migrate about:Networking to Fluent, part {index}. """

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutNetworking.ftl",
        "toolkit/toolkit/about/aboutNetworking.ftl",
        transforms_from(
"""
title = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.title")}
warning = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.warning")}
show-next-time-checkbox = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.showNextTime")}
ok = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.ok")}
http = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.HTTP")}
sockets = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.sockets")}
dns = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.dns")}
websockets = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.websockets")}
refresh = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.refresh")}
auto-refresh = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.autoRefresh")}
hostname = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.hostname")}
port = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.port")}
http2 = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.http2")}
ssl = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.ssl")}
active = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.active")}
idle = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.idle")}
host = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.host")}
tcp = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.tcp")}
sent = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.sent")}
received = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.received")}
family = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.family")}
trr = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.trr")}
addresses = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.addresses")}
expires = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.expires")}
messages-sent = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.messagesSent")}
messages-received = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.messagesReceived")}
bytes-sent = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.bytesSent")}
bytes-received = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.bytesReceived")}
logging = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.logging")}
current-log-file = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.currentLogFile")}
current-log-modules = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.currentLogModules")}
set-log-file = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.setLogFile")}
set-log-modules = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.setLogModules")}
start-logging = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.startLogging")}
stop-logging = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.stopLogging")}
dns-lookup = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.dnsLookup")}
dns-lookup-button = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.dnsLookupButton")}
dns-lookup-table-column = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.dnsLookupTableColumn")}
rcwn = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.rcwn")}
rcwn-status = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.rcwnStatus")}
rcwn-cache-won-count = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.rcwnCacheWonCount")}
rcwn-net-won-count = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.rcwnNetWonCount")}
total-network-requests = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.totalNetworkRequests")}
rcwn-operation = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.rcwnOperation")}
rcwn-perf-open = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.rcwnPerfOpen")}
rcwn-perf-read = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.rcwnPerfRead")}
rcwn-perf-write = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.rcwnPerfWrite")}
rcwn-perf-entry-open = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.rcwnPerfEntryOpen")}
rcwn-avg-short = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.rcwnAvgShort")}
rcwn-avg-long = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.rcwnAvgLong")}
rcwn-std-dev-long = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.rcwnStddevLong")}
rcwn-cache-slow = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.rcwnCacheSlow")}
rcwn-cache-not-slow = { COPY("toolkit/chrome/global/aboutNetworking.dtd", "aboutNetworking.rcwnCacheNotSlow")}
"""
        )
    )

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutNetworking.ftl",
        "toolkit/toolkit/about/aboutNetworking.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("log-tutorial"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutNetworking.dtd",
                    "aboutNetworking.logTutorial",
                    {
                        "href='https://developer.mozilla.org/docs/Mozilla/Debugging/HTTP_logging'": FTL.TextElement('data-l10n-name="logging"')
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("dns-domain"),
                value=CONCAT(
                    COPY(
                        "toolkit/chrome/global/aboutNetworking.dtd",
                        "aboutNetworking.dnsDomain",
                    ),
                    FTL.TextElement(":"),
                )
            )
        ]
    )
