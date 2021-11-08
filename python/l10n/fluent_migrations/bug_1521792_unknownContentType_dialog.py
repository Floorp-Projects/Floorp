# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, TERM_REFERENCE
from fluent.migrate import COPY, REPLACE


def migrate(ctx):
    """Bug 1521792 - Migrate unknownContentType dialog to FTL, part {index}"""

    ctx.add_transforms(
        "toolkit/toolkit/global/unknownContentType.ftl",
        "toolkit/toolkit/global/unknownContentType.ftl",
        transforms_from(
            """

unknowncontenttype-intro = { COPY(path1, "intro2.label") }
unknowncontenttype-which-is = { COPY(path1, "whichIs.label") }
unknowncontenttype-from = { COPY(path1, "from.label") }
unknowncontenttype-prompt = { COPY(path1, "unknownPromptText.label") }
""",
            path1="toolkit/chrome/mozapps/downloads/unknownContentType.dtd",
        )
        + [
            FTL.Message(
                id=FTL.Identifier("unknowncontenttype-action-question"),
                value=REPLACE(
                    "toolkit/chrome/mozapps/downloads/unknownContentType.dtd",
                    "actionQuestion.label",
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
        ]
        + transforms_from(
            """
unknowncontenttype-open-with =
    .label = { COPY(path1, "openWith.label") }
    .accesskey = { COPY(path1, "openWith.accesskey") }
unknowncontenttype-other =
    .label = { COPY(path1, "other.label") }
unknowncontenttype-choose-handler =
    .label =
        { PLATFORM() ->
            [macos] { COPY(path1, "chooseHandlerMac.label") }
           *[other] { COPY(path1, "chooseHandler.label") }
        }
    .accesskey =
        { PLATFORM() ->
            [macos] { COPY(path1, "chooseHandlerMac.accesskey") }
           *[other] { COPY(path1, "chooseHandler.accesskey") }
        }
unknowncontenttype-save-file =
    .label = { COPY(path1, "saveFile.label") }
    .accesskey = { COPY(path1, "saveFile.accesskey") }
unknowncontenttype-remember-choice =
    .label = { COPY(path1, "rememberChoice.label") }
    .accesskey = { COPY(path1, "rememberChoice.accesskey") }
""",
            path1="toolkit/chrome/mozapps/downloads/unknownContentType.dtd",
        ),
    )
