#!/usr/bin/env python
# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file generates the certspec files for test_cert_version.js. The naming
# convention for those files is generally of the form
# "<subject-description>_<issuer-description>.pem.certspec". End-entity
# certificates are generally called "ee". Intermediates are called
# "int". The root CA is called "ca" and self-signed certificates are called
# "ss".
# In the case that the subject and issuer are the same, the redundant part is
# not repeated.
# If there is nothing particularly special about a certificate, it has no
# description ("nothing particularly special" meaning the certificate is X509v3
# and has or does not have the basic constraints extension as expected by where
# it is in the hierarchy). Otherwise, the description includes its version and
# details about the extension. If the extension is not present, the string
# "noBC" is used. If it is present but the cA bit is not asserted, the string
# "BC-not-cA" is used. If it is present with the cA bit asserted, the string
# "BC-cA" is used.
# For example, a v1 intermediate that does not have the extension that was
# issued by the root CA has the name "int-v1-noBC_ca.pem.certspec".
# A v4 end-entity that does have the extension but does not assert the cA bit
# that was issued by the root CA has the name
# "ee-v4-BC-not-cA_ca.pem.certspec".
# An end-entity issued by a v3 intermediate with the extension that asserts the
# cA bit has the name "ee_int-v3-BC-cA.pem.certspec".

versions = {
    'v1': 1,
    'v2': 2,
    'v3': 3,
    'v4': 4
}

basicConstraintsTypes = {
    'noBC': '',
    'BC-not-cA': 'extension:basicConstraints:,',
    'BC-cA': 'extension:basicConstraints:cA,'
}

def writeCertspec(issuer, subject, fields):
    filename = '%s_%s.pem.certspec' % (subject, issuer)
    if issuer == subject:
        filename = '%s.pem.certspec' % subject
    with open(filename, 'w') as f:
        f.write('issuer:%s\n' % issuer)
        f.write('subject:%s\n' % subject)
        for field in fields:
            if len(field) > 0:
                f.write('%s\n' % field)


keyUsage = 'extension:keyUsage:keyCertSign,cRLSign'
basicConstraintsCA = 'extension:basicConstraints:cA,'

writeCertspec('ca', 'ca', [keyUsage, basicConstraintsCA])

for versionStr, versionVal in versions.iteritems():
    # intermediates
    versionText = 'version:%s' % versionVal
    for basicConstraintsType, basicConstraintsExtension in basicConstraintsTypes.iteritems():
        intermediateName = 'int-%s-%s' % (versionStr, basicConstraintsType)
        writeCertspec('ca', intermediateName,
                      [keyUsage, versionText, basicConstraintsExtension])
        writeCertspec(intermediateName, 'ee', [])

    # end-entities
    versionText = 'version:%s' % versionVal
    for basicConstraintsType, basicConstraintsExtension in basicConstraintsTypes.iteritems():
        writeCertspec('ca', 'ee-%s-%s' % (versionStr, basicConstraintsType),
                      [versionText, basicConstraintsExtension])

    # self-signed certificates
    versionText = 'version:%s' % versionVal
    for basicConstraintsType, basicConstraintsExtension in basicConstraintsTypes.iteritems():
        selfSignedName = 'ss-%s-%s' % (versionStr, basicConstraintsType)
        writeCertspec(selfSignedName, selfSignedName,
                      [versionText, basicConstraintsExtension])
