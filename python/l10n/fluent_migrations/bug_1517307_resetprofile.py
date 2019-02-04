# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import MESSAGE_REFERENCE, TERM_REFERENCE
from fluent.migrate import COPY, CONCAT, REPLACE


def migrate(ctx):
	"""Bug 1517307 - Convert resetProfile.dtd to Fluent, part {index}."""

	ctx.add_transforms(
		"toolkit/toolkit/global/resetProfile.ftl",
		"toolkit/toolkit/global/resetProfile.ftl",
		transforms_from(
"""
refresh-profile-description = { COPY("toolkit/chrome/global/resetProfile.dtd","refreshProfile.dialog.description1") }
refresh-profile-description-details = { COPY("toolkit/chrome/global/resetProfile.dtd","refreshProfile.dialog.description2") }
refresh-profile-remove = { COPY("toolkit/chrome/global/resetProfile.dtd","refreshProfile.dialog.items.label1") }
refresh-profile-restore = { COPY("toolkit/chrome/global/resetProfile.dtd","refreshProfile.dialog.items.label2") }
""")
)
	ctx.add_transforms(
		"toolkit/toolkit/global/resetProfile.ftl",
		"toolkit/toolkit/global/resetProfile.ftl",
		[
		FTL.Message(
			id=FTL.Identifier("refresh-profile-dialog"),
			attributes=[
				FTL.Attribute(
					FTL.Identifier("title"),
					value=REPLACE(
						"toolkit/chrome/global/resetProfile.dtd",
						"refreshProfile.dialog.title",
						{
							"&brandShortName;": TERM_REFERENCE("brand-short-name")
						}
					)
				)
			]
		),
		FTL.Message(
			id=FTL.Identifier("refresh-profile-dialog-button"),
			attributes=[
				FTL.Attribute(
					FTL.Identifier("label"),
					value=REPLACE(
						"toolkit/chrome/global/resetProfile.dtd",
						"refreshProfile.dialog.button.label",
						{
							"&brandShortName;": TERM_REFERENCE("brand-short-name")
						}
					)
				)
			]
		),
		FTL.Message(
			id=FTL.Identifier("refresh-profile"),
			value=REPLACE(
				"toolkit/chrome/global/resetProfile.dtd",
				"refreshProfile.title",
				{
					"&brandShortName;": TERM_REFERENCE("brand-short-name")
				}
			)
		),
		FTL.Message(
			id=FTL.Identifier("refresh-profile-button"),
			value=REPLACE(
				"toolkit/chrome/global/resetProfile.dtd",
				"refreshProfile.button.label",
				{
					"&brandShortName;": TERM_REFERENCE("brand-short-name")
				}
			) 
		),
	]
)
