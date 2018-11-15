from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate.helpers import MESSAGE_REFERENCE
from fluent.migrate import REPLACE
from fluent.migrate import COPY
from fluent.migrate import CONCAT

def migrate(ctx):
    """Bug 1501900 -  Migrate about:devtools to Fluent, part {index}"""

    ctx.add_transforms(
        "devtools/startup/aboutDevTools.ftl",
        "devtools/startup/aboutDevTools.ftl",
        transforms_from(
"""
head-title = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.headTitle")}
enable-title = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.enable.title")}
enable-inspect-element-title = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.enable.inspectElementTitle")}
enable-inspect-element-message = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.enable.inspectElementMessage")}
enable-about-debugging-message ={ COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.enable.aboutDebuggingMessage")}
enable-key-shortcut-message = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.enable.keyShortcutMessage")}
enable-menu-message = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.enable.menuMessage2")}
enable-common-message = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.enable.commonMessage")}
enable-learn-more-link = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.enable.learnMoreLink")}
enable-enable-button = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.enable.enableButton")}
enable-close-button = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.enable.closeButton2")}
welcome-title = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.welcome.title")}
newsletter-title = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.newsletter.title")}
newsletter-message = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.newsletter.message")}
newsletter-email-placeholder =
    .placeholder = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.newsletter.email.placeholder")}
newsletter-subscribe-button = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.newsletter.subscribeButton")}
newsletter-thanks-title = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.newsletter.thanks.title")}
newsletter-thanks-message = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.newsletter.thanks.message")}
footer-title = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.footer.title")}
footer-message = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.footer.message")}
footer-learn-more-link = { COPY("devtools/startup/aboutdevtools.dtd", "aboutDevtools.footer.learnMoreLink")}
features-learn-more = { COPY("devtools/startup/aboutdevtools.properties", "features.learnMore")}
features-inspector-title = { COPY("devtools/startup/aboutdevtools.properties", "features.inspector.title")}
features-console-title = { COPY("devtools/startup/aboutdevtools.properties", "features.console.title")}
features-debugger-title = { COPY("devtools/startup/aboutdevtools.properties", "features.debugger.title")}
features-network-title = { COPY("devtools/startup/aboutdevtools.properties", "features.network.title")}
features-storage-title = { COPY("devtools/startup/aboutdevtools.properties", "features.storage.title")}
features-responsive-title = { COPY("devtools/startup/aboutdevtools.properties", "features.responsive.title")}
features-visual-editing-title = { COPY("devtools/startup/aboutdevtools.properties", "features.visualediting.title")}
features-performance-title = { COPY("devtools/startup/aboutdevtools.properties", "features.performance.title")}
features-memory-title = { COPY("devtools/startup/aboutdevtools.properties", "features.memory.title")}
"""
        )
    )

    ctx.add_transforms(
        "devtools/startup/aboutDevTools.ftl",
        "devtools/startup/aboutDevTools.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("newsletter-privacy-label"),
                value=REPLACE(
                    "devtools/startup/aboutdevtools.dtd",
                    "aboutDevtools.newsletter.privacy.label",
                    {
                        "<a class='external' href='https://www.mozilla.org/privacy/'>": FTL.TextElement('<a data-l10n-name="privacy-policy">')
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("features-inspector-desc"),
                value=CONCAT(
                    COPY(
                        "devtools/startup/aboutdevtools.properties",
                        "features.inspector.desc",
                    ),
                    FTL.TextElement(' <a data-l10n-name="learn-more">'),
                    MESSAGE_REFERENCE("features-learn-more"),
                    FTL.TextElement("</a>")
                )
            ),
            FTL.Message(
                id=FTL.Identifier("features-console-desc"),
                value=CONCAT(
                    COPY(
                        "devtools/startup/aboutdevtools.properties",
                        "features.console.desc",
                    ),
                    FTL.TextElement(' <a data-l10n-name="learn-more">'),
                    MESSAGE_REFERENCE("features-learn-more"),
                    FTL.TextElement("</a>")
                )
            ),
            FTL.Message(
                id=FTL.Identifier("features-debugger-desc"),
                value=CONCAT(
                    COPY(
                        "devtools/startup/aboutdevtools.properties",
                        "features.debugger.desc",
                    ),
                    FTL.TextElement(' <a data-l10n-name="learn-more">'),
                    MESSAGE_REFERENCE("features-learn-more"),
                    FTL.TextElement("</a>")
                )
            ),
            FTL.Message(
                id=FTL.Identifier("features-network-desc"),
                value=CONCAT(
                    COPY(
                        "devtools/startup/aboutdevtools.properties",
                        "features.network.desc",
                    ),
                    FTL.TextElement(' <a data-l10n-name="learn-more">'),
                    MESSAGE_REFERENCE("features-learn-more"),
                    FTL.TextElement("</a>")
                )
            ),
            FTL.Message(
                id=FTL.Identifier("features-storage-desc"),
                value=CONCAT(
                    COPY(
                        "devtools/startup/aboutdevtools.properties",
                        "features.storage.desc",
                    ),
                    FTL.TextElement(' <a data-l10n-name="learn-more">'),
                    MESSAGE_REFERENCE("features-learn-more"),
                    FTL.TextElement("</a>")
                )
            ),
            FTL.Message(
                id=FTL.Identifier("features-responsive-desc"),
                value=CONCAT(
                    COPY(
                        "devtools/startup/aboutdevtools.properties",
                        "features.responsive.desc",
                    ),
                    FTL.TextElement(' <a data-l10n-name="learn-more">'),
                    MESSAGE_REFERENCE("features-learn-more"),
                    FTL.TextElement("</a>")
                )
            ),
            FTL.Message(
                id=FTL.Identifier("features-visual-editing-desc"),
                value=CONCAT(
                    COPY(
                        "devtools/startup/aboutdevtools.properties",
                        "features.visualediting.desc",
                    ),
                    FTL.TextElement(' <a data-l10n-name="learn-more">'),
                    MESSAGE_REFERENCE("features-learn-more"),
                    FTL.TextElement("</a>")
                )
            ),
            FTL.Message(
                id=FTL.Identifier("features-performance-desc"),
                value=CONCAT(
                    COPY(
                        "devtools/startup/aboutdevtools.properties",
                        "features.performance.desc",
                    ),
                    FTL.TextElement(' <a data-l10n-name="learn-more">'),
                    MESSAGE_REFERENCE("features-learn-more"),
                    FTL.TextElement("</a>")
                )
            ),
            FTL.Message(
                id=FTL.Identifier("features-memory-desc"),
                value=CONCAT(
                    COPY(
                        "devtools/startup/aboutdevtools.properties",
                        "features.memory.desc",
                    ),
                    FTL.TextElement(' <a data-l10n-name="learn-more">'),
                    MESSAGE_REFERENCE("features-learn-more"),
                    FTL.TextElement("</a>")
                )
            ),
            FTL.Message(
                id=FTL.Identifier("welcome-message"),
                value=REPLACE(
                    "devtools/startup/aboutdevtools.properties",
                    "welcome.message",
                    {
                        "%S": VARIABLE_REFERENCE("shortcut")
                    }
                )
            )
        ]
    )
