# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate import REPLACE


def migrate(ctx):
    """Bug 1486937 - Use fluent for about:url-classifier"""

    ctx.add_transforms(
        "toolkit/toolkit/about/url-classifier.ftl",
        "toolkit/toolkit/about/url-classifier.ftl",
        transforms_from(
"""
url-classifier-title = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.title")}
url-classifier-provider-title = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.providerTitle")}
url-classifier-provider = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.provider")}
url-classifier-provider-last-update-time = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.providerLastUpdateTime")}
url-classifier-provider-next-update-time = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.providerNextUpdateTime")}
url-classifier-provider-back-off-time = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.providerBackOffTime")}
url-classifier-provider-last-update-status = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.providerLastUpdateStatus")}
url-classifier-provider-update-btn = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.providerUpdateBtn")}
url-classifier-cache-title = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.cacheTitle")}
url-classifier-cache-refresh-btn = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.cacheRefreshBtn")}
url-classifier-cache-clear-btn = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.cacheClearBtn")}
url-classifier-cache-table-name = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.cacheTableName")}
url-classifier-cache-ncache-entries = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.cacheNCacheEntries")}
url-classifier-cache-pcache-entries = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.cachePCacheEntries")}
url-classifier-cache-show-entries = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.cacheShowEntries")}
url-classifier-cache-entries = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.cacheEntries")}
url-classifier-cache-prefix = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.cachePrefix")}
url-classifier-cache-ncache-expiry = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.cacheNCacheExpiry")}
url-classifier-cache-fullhash = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.cacheFullhash")}
url-classifier-cache-pcache-expiry = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.cachePCacheExpiry")}
url-classifier-debug-title = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.debugTitle")}
url-classifier-debug-module-btn = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.debugModuleBtn")}
url-classifier-debug-file-btn = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.debugFileBtn")}
url-classifier-debug-js-log-chk = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.debugJSLogChk")}
url-classifier-debug-sb-modules = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.debugSBModules")}
url-classifier-debug-modules = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.debugModules")}
url-classifier-debug-sbjs-modules = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.debugSBJSModules")}
url-classifier-debug-file = { COPY("toolkit/chrome/global/aboutUrlClassifier.dtd", "aboutUrlClassifier.debugFile")}

url-classifier-trigger-update = { COPY("toolkit/chrome/global/aboutUrlClassifier.properties", "TriggerUpdate")}
url-classifier-not-available = { COPY("toolkit/chrome/global/aboutUrlClassifier.properties", "NotAvailable")}
url-classifier-disable-sbjs-log = { COPY("toolkit/chrome/global/aboutUrlClassifier.properties", "DisableSBJSLog")}
url-classifier-enable-sbjs-log = { COPY("toolkit/chrome/global/aboutUrlClassifier.properties", "EnableSBJSLog")}
url-classifier-enabled = { COPY("toolkit/chrome/global/aboutUrlClassifier.properties", "Enabled")}
url-classifier-disabled = { COPY("toolkit/chrome/global/aboutUrlClassifier.properties", "Disabled")}
url-classifier-updating = { COPY("toolkit/chrome/global/aboutUrlClassifier.properties", "Updating")}
url-classifier-cannot-update = { COPY("toolkit/chrome/global/aboutUrlClassifier.properties", "CannotUpdate")}
url-classifier-success = { COPY("toolkit/chrome/global/aboutUrlClassifier.properties", "success")}
""")
    )

    ctx.add_transforms(
    "toolkit/toolkit/about/url-classifier.ftl",
    "toolkit/toolkit/about/url-classifier.ftl",
    [
        FTL.Message(
            id=FTL.Identifier("url-classifier-update-error"),
            value=REPLACE(
                "toolkit/chrome/global/aboutUrlClassifier.properties",
                "updateError",
                {
                    "%S": VARIABLE_REFERENCE(
                        "error"
                    ),
                }
            )
        ),
        FTL.Message(
            id=FTL.Identifier("url-classifier-download-error"),
            value=REPLACE(
                "toolkit/chrome/global/aboutUrlClassifier.properties",
                "downloadError",
                {
                    "%S": VARIABLE_REFERENCE(
                        "error"
                    ),
                }
            )
        ),
    ]
)
