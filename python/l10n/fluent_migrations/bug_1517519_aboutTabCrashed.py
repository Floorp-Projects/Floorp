# -*- coding: utf-8 -*-

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import REPLACE
from fluent.migrate import COPY
from fluent.migrate import CONCAT
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate.helpers import MESSAGE_REFERENCE


def migrate(ctx):
    """Bug 1517519 - Migrate aboutTabCrashed to Fluent, part {index}."""
    
    
    ctx.add_transforms(
        "browser/browser/aboutTabCrashed.ftl",
        "browser/browser/aboutTabCrashed.ftl",
        transforms_from(
                
"""
crashed-title = { COPY("browser/chrome/browser/aboutTabCrashed.dtd", "tabCrashed.title")}
crashed-close-tab-button = { COPY("browser/chrome/browser/aboutTabCrashed.dtd", "tabCrashed.closeTab2")}
crashed-restore-tab-button = { COPY("browser/chrome/browser/aboutTabCrashed.dtd", "tabCrashed.restoreTab")}
crashed-restore-all-button = { COPY("browser/chrome/browser/aboutTabCrashed.dtd", "tabCrashed.restoreAll")}
crashed-header ={ COPY("browser/chrome/browser/aboutTabCrashed.dtd", "tabCrashed.header2")}
crashed-offer-help = { COPY("browser/chrome/browser/aboutTabCrashed.dtd", "tabCrashed.offerHelp2")}
crashed-request-help = { COPY("browser/chrome/browser/aboutTabCrashed.dtd", "tabCrashed.requestHelp")}
crashed-request-report-title = { COPY("browser/chrome/browser/aboutTabCrashed.dtd", "tabCrashed.requestReport")}
crashed-send-report = { COPY("browser/chrome/browser/aboutTabCrashed.dtd", "tabCrashed.sendReport3")}
crashed-comment-placeholder = { COPY("browser/chrome/browser/aboutTabCrashed.dtd", "tabCrashed.commentPlaceholder2")}
crashed-email-placeholder = { COPY("browser/chrome/browser/aboutTabCrashed.dtd", "tabCrashed.emailPlaceholder")}
crashed-email-me = { COPY("browser/chrome/browser/aboutTabCrashed.dtd", "tabCrashed.emailMe")}
crashed-request-auto-submit-title = { COPY("browser/chrome/browser/aboutTabCrashed.dtd", "tabCrashed.requestAutoSubmit2")}

"""
        )
    )
	
	
    ctx.add_transforms(
        "browser/browser/aboutTabCrashed.ftl",
        "browser/browser/aboutTabCrashed.ftl",
        [
			FTL.Message(
				id=FTL.Identifier("crashed-single-offer-help-message"),
				value=REPLACE(
					"browser/chrome/browser/aboutTabCrashed.dtd",
					"tabCrashed.single.offerHelpMessage2",
					{
						"&tabCrashed.restoreTab;" : MESSAGE_REFERENCE("crashed-restore-tab-button")
               
					}
				)
			),
			
			FTL.Message(
				id=FTL.Identifier("crashed-multiple-offer-help-message"),
				value=REPLACE(
					"browser/chrome/browser/aboutTabCrashed.dtd",
					"tabCrashed.multiple.offerHelpMessage2",
					{
						"&tabCrashed.restoreTab;" : MESSAGE_REFERENCE("crashed-restore-tab-button"),
						"&tabCrashed.restoreAll;" : MESSAGE_REFERENCE("crashed-restore-all-button")
					}
				)
			),
			
			FTL.Message(
				id=FTL.Identifier("crashed-request-help-message"),
				value=REPLACE(
					"browser/chrome/browser/aboutTabCrashed.dtd",
					"tabCrashed.requestHelpMessage",
					{
						"&brandShortName;" : TERM_REFERENCE("brand-short-name")
					}
				)
			),
			
			FTL.Message(
				id=FTL.Identifier("crashed-include-URL"),
				value=REPLACE(
					"browser/chrome/browser/aboutTabCrashed.dtd",
					"tabCrashed.includeURL3",
					{
						"&brandShortName;" : TERM_REFERENCE("brand-short-name")
					}
				)
			),
			
			FTL.Message(
				id=FTL.Identifier("crashed-report-sent"),
				value=REPLACE(
					"browser/chrome/browser/aboutTabCrashed.dtd",
					"tabCrashed.reportSent",
					{
						"&brandShortName;" : TERM_REFERENCE("brand-short-name")
					}
				)
			),
			
			FTL.Message(
				id=FTL.Identifier("crashed-auto-submit-checkbox"),
				value=REPLACE(
					"browser/chrome/browser/aboutTabCrashed.dtd",
					"tabCrashed.autoSubmit3",
					{
						"&brandShortName;" : TERM_REFERENCE("brand-short-name")
					}
				)
			)
		]
    )
        
