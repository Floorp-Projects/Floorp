# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import COPY
from fluent.migrate import REPLACE
from fluent.migrate import CONCAT
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import MESSAGE_REFERENCE
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate.helpers import VARIABLE_REFERENCE

def migrate(ctx):
    """Bug 1519923 - Migrate about:rights to Fluent, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutRights.ftl",
        "toolkit/toolkit/about/aboutRights.ftl",
        transforms_from(
"""
rights-title = { COPY(from_path, "rights.title") }
rights-intro-point-4-unbranded = { COPY(from_path, "rights.intro-point3-unbranded") }
rights-safebrowsing-term-1 = { COPY(from_path, "rights.safebrowsing-term1") }
rights-safebrowsing-term-2 = { COPY(from_path, "rights.safebrowsing-term2") }
rights-safebrowsing-term-4 = { COPY(from_path, "rights.safebrowsing-term4") }
rights-locationawarebrowsing-term-2 = { COPY(from_path, "rights.locationawarebrowsing-term2") }
rights-locationawarebrowsing-term-3 = { COPY(from_path, "rights.locationawarebrowsing-term3") }
rights-locationawarebrowsing-term-4 = { COPY(from_path, "rights.locationawarebrowsing-term4") }
rights-webservices-unbranded = { COPY(from_path, "rights.webservices-unbranded") }
rights-webservices-term-unbranded = { COPY(from_path, "rights.webservices-term1-unbranded") }
rights-webservices-term-7 = { COPY(from_path, "rights.webservices-term7") }
enableSafeBrowsing-label = { COPY("browser/chrome/browser/preferences/security.dtd", "enableSafeBrowsing.label")}
""", from_path="toolkit/chrome/global/aboutRights.dtd"
        )
    )

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutRights.ftl",
        "toolkit/toolkit/about/aboutRights.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("rights-intro"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutRights.dtd",
                    "rights.intro",
                    {
                        "&brandFullName;": TERM_REFERENCE("brand-full-name")
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-intro-point-1"),
                value=CONCAT(
                    REPLACE(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.intro-point1a",
                        {
                            "&brandShortName;": TERM_REFERENCE("brand-short-name")
                        }
                    ),
                    FTL.TextElement('<a data-l10n-name="mozilla-public-license-link">'),
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.intro-point1b"
                    ),
                    FTL.TextElement('</a>'),
                    REPLACE(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.intro-point1c",
                        {
                            "&brandShortName;": TERM_REFERENCE("brand-short-name")
                        }
                    )
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-intro-point-2"),
                value=CONCAT(
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.intro-point2-a"
                    ),
                    FTL.TextElement('<a data-l10n-name="mozilla-trademarks-link">'),
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.intro-point2-b"
                    ),
                    FTL.TextElement('</a>'),
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.intro-point2-c"
                    )
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-intro-point-3"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutRights.dtd",
                    "rights.intro-point2.5",
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                        "&vendorShortName;": TERM_REFERENCE("vendor-short-name")
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-intro-point-4"),
                value=CONCAT(
                    REPLACE(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights2.intro-point3a",
                        {
                            "&vendorShortName;": TERM_REFERENCE("vendor-short-name"),
                            "&brandShortName;": TERM_REFERENCE("brand-short-name")
                        }
                    ),
                    FTL.TextElement('<a data-l10n-name="mozilla-privacy-policy-link">'),
                    REPLACE(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights2.intro-point3b",
                        {
                            "&brandShortName;": TERM_REFERENCE("brand-short-name")
                        }
                    ),
                    FTL.TextElement('</a>'),
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.intro-point3c"
                    )
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-intro-point-5"),
                value=CONCAT(
                    REPLACE(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights2.intro-point4a",
                        {
                            "&brandShortName;": TERM_REFERENCE("brand-short-name")
                        }
                    ),
                    FTL.TextElement('<a data-l10n-name="mozilla-service-terms-link">'),
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.intro-point4b"
                    ),
                    FTL.TextElement('</a>'),
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.intro-point4c"
                    )
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-intro-point-5-unbranded"),
                value=CONCAT(
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.intro-point4a-unbranded"
                    ),
                    FTL.TextElement('<a data-l10n-name="mozilla-website-services-link">'),
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.intro-point4b-unbranded"
                    ),
                    FTL.TextElement('</a>'),
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.intro-point4c-unbranded"
                    )
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-intro-point-6"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutRights.dtd",
                    "rights.intro-point5",
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name")
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-webservices-header"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutRights.dtd",
                    "rights2.webservices-header",
                    {
                        "&brandFullName;": TERM_REFERENCE("brand-full-name")
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-webservices"),
                value=CONCAT(
                    REPLACE(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights2.webservices-a",
                        {
                            "&brandFullName;": TERM_REFERENCE("brand-full-name"),
                            "&brandShortName;": TERM_REFERENCE("brand-short-name")
                        }
                    ),
                    FTL.TextElement('<a data-l10n-name="mozilla-disable-service-link">'),
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights2.webservices-b"
                    ),
                    FTL.TextElement('</a>'),
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights3.webservices-c"
                    )
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-safebrowsing"),
                value=CONCAT(
                    FTL.TextElement('<strong>'),
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.safebrowsing-a"
                    ),
                    FTL.TextElement('</strong>'),
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.safebrowsing-b"
                    )
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-safebrowsing-term-3"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutRights.dtd",
                    "rights2.safebrowsing-term3",
                    {
                        "&enableSafeBrowsing.label;": MESSAGE_REFERENCE("enableSafeBrowsing-label")
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-locationawarebrowsing"),
                value=CONCAT(
                    FTL.TextElement('<strong>'),
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.locationawarebrowsing-a"
                    ),
                    FTL.TextElement('</strong>'),
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.locationawarebrowsing-b"
                    )
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-locationawarebrowsing-term-1"),
                value=CONCAT(
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.locationawarebrowsing-term1a"
                    ),
                    FTL.TextElement('<code>'),
                    COPY(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.locationawarebrowsing-term1b"
                    ),
                    FTL.TextElement('</code>')
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-webservices-term-1"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutRights.dtd",
                    "rights2.webservices-term1",
                    {
                        "&vendorShortName;": TERM_REFERENCE("vendor-short-name")
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-webservices-term-2"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutRights.dtd",
                    "rights.webservices-term2",
                    {
                        "&vendorShortName;": TERM_REFERENCE("vendor-short-name")
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-webservices-term-3"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutRights.dtd",
                    "rights2.webservices-term3",
                    {
                        "&vendorShortName;": TERM_REFERENCE("vendor-short-name"),
                        "&brandShortName;": TERM_REFERENCE("brand-short-name")
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-webservices-term-4"),
                value=CONCAT(
                    FTL.TextElement('<strong>'),
                    REPLACE(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.webservices-term4",
                        {
                            "&vendorShortName;": TERM_REFERENCE("vendor-short-name"),
                            "&brandShortName;": TERM_REFERENCE("brand-short-name")
                        }
                    ),
                    FTL.TextElement('</strong>')
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-webservices-term-5"),
                value=CONCAT(
                    FTL.TextElement('<strong>'),
                    REPLACE(
                        "toolkit/chrome/global/aboutRights.dtd",
                        "rights.webservices-term5",
                        {
                            "&vendorShortName;": TERM_REFERENCE("vendor-short-name"),
                            "&brandShortName;": TERM_REFERENCE("brand-short-name")
                        }
                    ),
                    FTL.TextElement('</strong>')
                )
            ),
            FTL.Message(
                id=FTL.Identifier("rights-webservices-term-6"),
                value=REPLACE(
                    "toolkit/chrome/global/aboutRights.dtd",
                    "rights.webservices-term6",
                    {
                        "&vendorShortName;": TERM_REFERENCE("vendor-short-name")
                    }
                )
            )


        ]
    )
