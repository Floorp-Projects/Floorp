# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE, TERM_REFERENCE
from fluent.migrate import COPY, CONCAT, REPLACE

def migrate(ctx):
	"""Bug 1549561 - Migrate strings from pipnss.properties to aboutCertError.ftl"""
	ctx.add_transforms(
		'browser/browser/aboutCertError.ftl',
		'browser/browser/aboutCertError.ftl',
        transforms_from(
"""
cert-error-mitm-intro = { COPY(from_path, "certErrorMitM") }
cert-error-trust-unknown-issuer-intro = { COPY(from_path, "certErrorTrust_UnknownIssuer4") }
cert-error-trust-cert-invalid = { COPY(from_path, "certErrorTrust_CaInvalid") }
cert-error-trust-untrusted-issuer = { COPY(from_path, "certErrorTrust_Issuer") }
cert-error-trust-signature-algorithm-disabled = { COPY(from_path, "certErrorTrust_SignatureAlgorithmDisabled") }
cert-error-trust-expired-issuer = { COPY(from_path, "certErrorTrust_ExpiredIssuer") }
cert-error-trust-self-signed = { COPY(from_path, "certErrorTrust_SelfSigned") }
cert-error-trust-symantec = { COPY(from_path, "certErrorTrust_Symantec1") }
cert-error-untrusted-default = { COPY(from_path, "certErrorTrust_Untrusted") }
""", from_path="security/manager/chrome/pipnss/pipnss.properties"))
	ctx.add_transforms(
		'browser/browser/aboutCertError.ftl',
                'browser/browser/aboutCertError.ftl',
		[
			FTL.Message(
				id=FTL.Identifier('cert-error-intro'),
				value=REPLACE(
					'security/manager/chrome/pipnss/pipnss.properties',
					'certErrorIntro',
					{
						"%1$S": VARIABLE_REFERENCE("hostname"),
					},
					normalize_printf=True
				)
			),
			FTL.Message(
				id=FTL.Identifier('cert-error-mitm-mozilla'),
				value=REPLACE(
					'security/manager/chrome/pipnss/pipnss.properties',
					'certErrorMitM2',
					{
						"%1$S": TERM_REFERENCE("brand-short-name"),
					},
					normalize_printf=True
				)
			),
			FTL.Message(
				id=FTL.Identifier('cert-error-mitm-connection'),
				value=REPLACE(
					'security/manager/chrome/pipnss/pipnss.properties',
					'certErrorMitM3',
					{
						"%1$S": TERM_REFERENCE("brand-short-name"),
                                        },
                                        normalize_printf=True
                                )
                        ),
			FTL.Message(
				id=FTL.Identifier('cert-error-trust-unknown-issuer'),
				value=REPLACE(
					'security/manager/chrome/pipnss/pipnss.properties',
					'certErrorTrust_UnknownIssuer6',
					{
						"%1$S": TERM_REFERENCE("brand-short-name"),
						"%2$S": VARIABLE_REFERENCE("hostname"),
					},
					normalize_printf=True
                                )
                        ),
                        FTL.Message(
				id=FTL.Identifier('cert-error-domain-mismatch'),
				value=REPLACE(
					'security/manager/chrome/pipnss/pipnss.properties',
					'certErrorMismatch3',
					{
						"%1$S": TERM_REFERENCE("brand-short-name"),
                                                "%2$S": VARIABLE_REFERENCE("hostname"),
                                        },
                                        normalize_printf=True
                                )
                        ),
			FTL.Message(
                                id=FTL.Identifier('cert-error-domain-mismatch-single-nolink'),
				value=CONCAT(
					REPLACE(
						'security/manager/chrome/pipnss/pipnss.properties',
						'certErrorMismatchSinglePrefix3',
                                        	{
                                                	"%1$S": TERM_REFERENCE("brand-short-name"),
                                        	        "%2$S": VARIABLE_REFERENCE("hostname"),
                                        	},
                                        	normalize_printf=True
                                	),
					FTL.TextElement(' '),
					REPLACE(
						'security/manager/chrome/pipnss/pipnss.properties',
						'certErrorMismatchSinglePrefix',
						{
							"%1$S": VARIABLE_REFERENCE("alt-name"),
						},
						normalize_printf=True
                                        ),
                        	),
			),
			FTL.Message(
                                id=FTL.Identifier('cert-error-domain-mismatch-single'),
				value=CONCAT(
					REPLACE(
                                                'security/manager/chrome/pipnss/pipnss.properties',
                                                'certErrorMismatchSinglePrefix3',
                                                {
                                                        "%1$S": TERM_REFERENCE("brand-short-name"),
                                                        "%2$S": VARIABLE_REFERENCE("hostname"),
                                                },
                                                normalize_printf=True
                                        ),
					FTL.TextElement(' '),
					REPLACE(
                                                'security/manager/chrome/pipnss/pipnss.properties',
                                                'certErrorMismatchSinglePrefix',
                                                {
                                                        "%S": CONCAT(
								FTL.TextElement('<a data-l10n-name="domain-mismatch-link">'),
								VARIABLE_REFERENCE("alt-name"),
								FTL.TextElement('</a>'),
							),
                                                },
                                        ),
				),
			),
			FTL.Message(
                                id=FTL.Identifier('cert-error-domain-mismatch-multiple'),
				value=CONCAT(
					REPLACE(
						'security/manager/chrome/pipnss/pipnss.properties',
						'certErrorMismatchMultiple3',
						{
							"%1$S": TERM_REFERENCE("brand-short-name"),
                                                	"%2$S": VARIABLE_REFERENCE("hostname"),
						},
						normalize_printf=True
                                	),
					FTL.TextElement(' '),
					VARIABLE_REFERENCE("subject-alt-names"),
				),
			),
			FTL.Message(
                                id=FTL.Identifier('cert-error-expired-now'),
				value=REPLACE(
                                        'security/manager/chrome/pipnss/pipnss.properties',
					'certErrorExpiredNow3',
					{
						"%1$S": VARIABLE_REFERENCE("hostname"),
						"%2$S": VARIABLE_REFERENCE("not-after-local-time"),
					},
                                        normalize_printf=True
                                ),
                        ),
			FTL.Message(
                                id=FTL.Identifier('cert-error-not-yet-valid-now'),
				value=REPLACE(
                                        'security/manager/chrome/pipnss/pipnss.properties',
					'certErrorNotYetValidNow3',
					{
						"%1$S": VARIABLE_REFERENCE("hostname"),
                                                "%2$S": VARIABLE_REFERENCE("not-before-local-time"),
					},
                                        normalize_printf=True
                                ),
                        ),
			FTL.Message(
                                id=FTL.Identifier('cert-error-code-prefix-link'),
				value=REPLACE(
                                        'security/manager/chrome/pipnss/pipnss.properties',
					'certErrorCodePrefix3',
					{
						"%1$S": CONCAT(
							FTL.TextElement('<a data-l10n-name="error-code-link">'),
							VARIABLE_REFERENCE("error"),
							FTL.TextElement('</a>'),
						),
					},
					normalize_printf=True
				),
			),
		]
	)
