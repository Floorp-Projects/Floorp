# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
from fluent.migrate import REPLACE, COPY


def migrate(ctx):
    """Bug 1737043 - On "site not found" pages for "foo", put a suggestion for "www.foo.com" if it exists, part {index}"""
    source = "browser/chrome/overrides/netError.dtd"
    target = "browser/browser/netError.ftl"
    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
dns-not-found-title =
    { COPY(from_path, "dnsNotFound.pageTitle") }
""",
            from_path=source,
        ),
    )
