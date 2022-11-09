# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, REPLACE, Transform


def migrate(ctx):
    """Bug 1791178 - Convert DownloadUIHelper.jsm to Fluent, part {index}."""

    source = "toolkit/chrome/mozapps/downloads/downloads.properties"
    target = "toolkit/toolkit/downloads/downloadUI.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("download-ui-confirm-title"),
                value=COPY(source, "quitCancelDownloadsAlertTitle"),
            ),
            FTL.Message(
                id=FTL.Identifier("download-ui-confirm-quit-cancel-downloads"),
                value=Transform.pattern_of(
                    FTL.SelectExpression(
                        selector=VARIABLE_REFERENCE("downloadsCount"),
                        variants=[
                            FTL.Variant(
                                key=FTL.Identifier("1"),
                                value=COPY(source, "quitCancelDownloadsAlertMsg"),
                            ),
                            FTL.Variant(
                                key=FTL.Identifier("other"),
                                default=True,
                                value=REPLACE(
                                    source,
                                    "quitCancelDownloadsAlertMsgMultiple",
                                    {"%1$S": VARIABLE_REFERENCE("downloadsCount")},
                                ),
                            ),
                        ],
                    )
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("download-ui-confirm-quit-cancel-downloads-mac"),
                value=Transform.pattern_of(
                    FTL.SelectExpression(
                        selector=VARIABLE_REFERENCE("downloadsCount"),
                        variants=[
                            FTL.Variant(
                                key=FTL.Identifier("1"),
                                value=COPY(source, "quitCancelDownloadsAlertMsgMac"),
                            ),
                            FTL.Variant(
                                key=FTL.Identifier("other"),
                                default=True,
                                value=REPLACE(
                                    source,
                                    "quitCancelDownloadsAlertMsgMacMultiple",
                                    {"%1$S": VARIABLE_REFERENCE("downloadsCount")},
                                ),
                            ),
                        ],
                    )
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("download-ui-confirm-offline-cancel-downloads"),
                value=Transform.pattern_of(
                    FTL.SelectExpression(
                        selector=VARIABLE_REFERENCE("downloadsCount"),
                        variants=[
                            FTL.Variant(
                                key=FTL.Identifier("1"),
                                value=COPY(source, "offlineCancelDownloadsAlertMsg"),
                            ),
                            FTL.Variant(
                                key=FTL.Identifier("other"),
                                default=True,
                                value=REPLACE(
                                    source,
                                    "offlineCancelDownloadsAlertMsgMultiple",
                                    {"%1$S": VARIABLE_REFERENCE("downloadsCount")},
                                ),
                            ),
                        ],
                    )
                ),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "download-ui-confirm-leave-private-browsing-windows-cancel-downloads"
                ),
                value=Transform.pattern_of(
                    FTL.SelectExpression(
                        selector=VARIABLE_REFERENCE("downloadsCount"),
                        variants=[
                            FTL.Variant(
                                key=FTL.Identifier("1"),
                                value=COPY(
                                    source,
                                    "leavePrivateBrowsingWindowsCancelDownloadsAlertMsg2",
                                ),
                            ),
                            FTL.Variant(
                                key=FTL.Identifier("other"),
                                default=True,
                                value=REPLACE(
                                    source,
                                    "leavePrivateBrowsingWindowsCancelDownloadsAlertMsgMultiple2",
                                    {"%1$S": VARIABLE_REFERENCE("downloadsCount")},
                                ),
                            ),
                        ],
                    )
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("download-ui-cancel-downloads-ok"),
                value=Transform.pattern_of(
                    FTL.SelectExpression(
                        selector=VARIABLE_REFERENCE("downloadsCount"),
                        variants=[
                            FTL.Variant(
                                key=FTL.Identifier("1"),
                                value=COPY(source, "cancelDownloadsOKText"),
                            ),
                            FTL.Variant(
                                key=FTL.Identifier("other"),
                                default=True,
                                value=REPLACE(
                                    source,
                                    "cancelDownloadsOKTextMultiple",
                                    {"%1$S": VARIABLE_REFERENCE("downloadsCount")},
                                ),
                            ),
                        ],
                    )
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("download-ui-dont-quit-button"),
                value=Transform.pattern_of(
                    FTL.SelectExpression(
                        selector=FTL.FunctionReference(
                            id=FTL.Identifier("PLATFORM"), arguments=FTL.CallArguments()
                        ),
                        variants=[
                            FTL.Variant(
                                key=FTL.Identifier("mac"),
                                value=COPY(source, "dontQuitButtonMac"),
                            ),
                            FTL.Variant(
                                key=FTL.Identifier("other"),
                                default=True,
                                value=COPY(source, "dontQuitButtonWin"),
                            ),
                        ],
                    )
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("download-ui-dont-go-offline-button"),
                value=COPY(source, "dontGoOfflineButton"),
            ),
            FTL.Message(
                id=FTL.Identifier("download-ui-dont-leave-private-browsing-button"),
                value=COPY(source, "dontLeavePrivateBrowsingButton2"),
            ),
            FTL.Message(
                id=FTL.Identifier("download-ui-file-executable-security-warning-title"),
                value=COPY(source, "fileExecutableSecurityWarningTitle"),
            ),
            FTL.Message(
                id=FTL.Identifier("download-ui-file-executable-security-warning"),
                value=REPLACE(
                    source,
                    "fileExecutableSecurityWarning",
                    {
                        "%1$S": VARIABLE_REFERENCE("executable"),
                        "%2$S": VARIABLE_REFERENCE("executable"),
                    },
                ),
            ),
        ],
    )
