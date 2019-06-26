# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate import COPY, REPLACE

def migrate(ctx):
	"""Bug 1555438 - Migrate strings from pipnss.properties to aboutCertError.ftl"""
	ctx.add_transforms(
                'browser/browser/aboutCertError.ftl',
                'browser/browser/aboutCertError.ftl',
		[
                        FTL.Message(
				id=FTL.Identifier('cert-error-details-hsts-label'),
				value=REPLACE(
					'browser/chrome/browser/browser.properties',
					'certErrorDetailsHSTS.label',
					{
						"%1$S": VARIABLE_REFERENCE("hasHSTS"),
					},
					normalize_printf=True
                                ),
                        ),
			FTL.Message(
                                id=FTL.Identifier('cert-error-details-key-pinning-label'),
				value=REPLACE(
                                        'browser/chrome/browser/browser.properties',
                                        'certErrorDetailsKeyPinning.label',
					{
						"%1$S": VARIABLE_REFERENCE("hasHPKP"),
					},
					normalize_printf=True
                                ),
                        ),
		]
	)
	ctx.add_transforms(
		'browser/browser/aboutCertError.ftl',
		'browser/browser/aboutCertError.ftl',
        transforms_from(
"""
cert-error-details-cert-chain-label = { COPY(from_path, "certErrorDetailsCertChain.label") }
""", from_path="browser/chrome/browser/browser.properties"))
