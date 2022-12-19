# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate import COPY_PATTERN
import fluent.syntax.ast as FTL


def migrate(ctx):
    """Bug 1805319 - Fix typo in webrtc indicator message id, part {index}."""

    path = "browser/browser/webrtcIndicator.ftl"
    ctx.add_transforms(
        path,
        path,
        [
            FTL.Message(
                id=FTL.Identifier("webrtc-allow-share-microphone-unsafe-delegation"),
                value=COPY_PATTERN(
                    path, "webrtc-allow-share-microphone-unsafe-delegations"
                ),
            ),
        ],
    )
