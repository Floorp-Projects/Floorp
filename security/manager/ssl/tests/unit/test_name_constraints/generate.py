#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

def writeSpecification(subject, issuer, extensions, fileBase):
    with open('%s.pem.certspec' % fileBase, 'w+') as f:
        f.write('issuer:%s\n' % issuer)
        f.write('subject:%s\n' % subject)
        if extensions:
            f.write('%s\n' % extensions)

def generateCommonEndEntityCertificates(issuer, issuerFileBase):
    writeSpecification('www.foo.com', issuer, None, 'cn-www.foo.com-%s' % issuerFileBase)
    writeSpecification('www.foo.org', issuer, None, 'cn-www.foo.org-%s' % issuerFileBase)
    writeSpecification('www.foo.com', issuer, 'extension:subjectAlternativeName:*.foo.org',
        'cn-www.foo.com-alt-foo.org-%s' % issuerFileBase)
    writeSpecification('www.foo.org', issuer, 'extension:subjectAlternativeName:*.foo.com',
        'cn-www.foo.org-alt-foo.com-%s' % issuerFileBase)
    writeSpecification('www.foo.com', issuer, 'extension:subjectAlternativeName:*.foo.com',
        'cn-www.foo.com-alt-foo.com-%s' % issuerFileBase)
    writeSpecification('www.foo.org', issuer, 'extension:subjectAlternativeName:*.foo.org',
        'cn-www.foo.org-alt-foo.org-%s' % issuerFileBase)
    writeSpecification('www.foo.com', issuer,
        'extension:subjectAlternativeName:*.foo.com,*.a.a.us,*.b.a.us',
        'cn-www.foo.com-alt-foo.com-a.a.us-b.a.us-%s' % issuerFileBase)
    writeSpecification('/C=US/O=bar/CN=www.foo.com', issuer, None,
        'cn-www.foo.com_o-bar_c-us-%s' % issuerFileBase)
    writeSpecification('/C=US/O=bar/CN=www.foo.org', issuer, None,
        'cn-www.foo.org_o-bar_c-us-%s' % issuerFileBase)
    writeSpecification('/C=US/O=bar/CN=www.foo.com', issuer,
        'extension:subjectAlternativeName:*.foo.org',
        'cn-www.foo.com_o-bar_c-us-alt-foo.org-%s' % issuerFileBase)
    writeSpecification('/C=US/O=bar/CN=www.foo.org', issuer,
        'extension:subjectAlternativeName:*.foo.com',
        'cn-www.foo.org_o-bar_c-us-alt-foo.com-%s' % issuerFileBase)
    writeSpecification('/C=US/O=bar/CN=www.foo.com', issuer,
        'extension:subjectAlternativeName:*.foo.com',
        'cn-www.foo.com_o-bar_c-us-alt-foo.com-%s' % issuerFileBase)
    writeSpecification('/C=US/O=bar/CN=www.foo.org', issuer,
        'extension:subjectAlternativeName:*.foo.org',
        'cn-www.foo.org_o-bar_c-us-alt-foo.org-%s' % issuerFileBase)
    writeSpecification('/C=US/O=bar/CN=www.foo.com', issuer,
        'extension:subjectAlternativeName:*.foo.com,*.a.a.us,*.b.a.us',
        'cn-www.foo.com_o-bar_c-us-alt-foo.com-a.a.us-b.a.us-%s' % issuerFileBase)

def generateCerts():
    # Unconstrainted root certificate to issue most intermediates
    caExtensions = 'extension:basicConstraints:cA,\nextension:keyUsage:cRLSign,keyCertSign'
    writeSpecification('ca-nc', 'ca-nc', caExtensions, 'ca-nc')

    # Intermediate with permitted subtree: dNSName:foo.com
    writeSpecification('int-nc-perm-foo.com-ca-nc', 'ca-nc',
        caExtensions + '\nextension:nameConstraints:permitted:foo.com',
        'int-nc-perm-foo.com-ca-nc')
    generateCommonEndEntityCertificates('int-nc-perm-foo.com-ca-nc', 'int-nc-perm-foo.com-ca-nc')

    # Intermediate with excluded subtree: dNSName:foo.com
    writeSpecification('int-nc-excl-foo.com-ca-nc', 'ca-nc',
        caExtensions + '\nextension:nameConstraints:excluded:foo.com',
        'int-nc-excl-foo.com-ca-nc')
    generateCommonEndEntityCertificates('int-nc-excl-foo.com-ca-nc', 'int-nc-excl-foo.com-ca-nc')

    # Intermediate with permitted subtree: directoryName:/C=US
    writeSpecification('int-nc-c-us-ca-nc', 'ca-nc',
        caExtensions + '\nextension:nameConstraints:permitted:/C=US', 'int-nc-c-us-ca-nc')
    generateCommonEndEntityCertificates('int-nc-c-us-ca-nc', 'int-nc-c-us-ca-nc')

    # Intermediate issued by previous intermediate with permitted subtree: dnsName:foo.com
    writeSpecification('/C=US/CN=int-nc-foo.com-int-nc-c-us-ca-nc', 'int-nc-c-us-ca-nc',
        caExtensions + '\nextension:nameConstraints:permitted:foo.com',
        'int-nc-foo.com-int-nc-c-us-ca-nc')
    generateCommonEndEntityCertificates('/C=US/CN=int-nc-foo.com-int-nc-c-us-ca-nc',
        'int-nc-foo.com-int-nc-c-us-ca-nc')

    # Intermediate with permitted subtree: dNSName:foo.com, directoryName:/C=US
    writeSpecification('int-nc-perm-foo.com_c-us-ca-nc', 'ca-nc',
        caExtensions + '\nextension:nameConstraints:permitted:foo.com,/C=US',
        'int-nc-perm-foo.com_c-us-ca-nc')
    generateCommonEndEntityCertificates('int-nc-perm-foo.com_c-us-ca-nc',
        'int-nc-perm-foo.com_c-us-ca-nc')

    # Intermediate with permitted subtree: directoryName:/C=UK
    writeSpecification('int-nc-perm-c-uk-ca-nc', 'ca-nc',
        caExtensions + '\nextension:nameConstraints:permitted:/C=UK', 'int-nc-perm-c-uk-ca-nc')
    generateCommonEndEntityCertificates('int-nc-perm-c-uk-ca-nc', 'int-nc-perm-c-uk-ca-nc')

    # Intermediate issued by previous intermediate in a different directoryName tree
    writeSpecification('/C=US/CN=int-c-us-int-nc-perm-c-uk-ca-nc', 'int-nc-perm-c-uk-ca-nc',
        caExtensions, 'int-c-us-int-nc-perm-c-uk-ca-nc')
    generateCommonEndEntityCertificates('/C=US/CN=int-c-us-int-nc-perm-c-uk-ca-nc',
        'int-c-us-int-nc-perm-c-uk-ca-nc')

    # Intermediate with permitted subtree: dNSName:foo.com, dNSName:a.us
    writeSpecification('int-nc-foo.com_a.us', 'ca-nc',
        caExtensions + '\nextension:nameConstraints:permitted:foo.com,a.us',
        'int-nc-foo.com_a.us')
    generateCommonEndEntityCertificates('int-nc-foo.com_a.us', 'int-nc-foo.com_a.us')

    # Intermediate issued by previous intermediate with permitted subtree: dNSName:foo.com
    writeSpecification('int-nc-foo.com-int-nc-foo.com_a.us', 'int-nc-foo.com_a.us',
        caExtensions + '\nextension:nameConstraints:permitted:foo.com',
        'int-nc-foo.com-int-nc-foo.com_a.us')
    generateCommonEndEntityCertificates('int-nc-foo.com-int-nc-foo.com_a.us',
        'int-nc-foo.com-int-nc-foo.com_a.us')

    # Root certificate with permitted subtree: dNSName:foo.com
    writeSpecification('ca-nc-perm-foo.com', 'ca-nc-perm-foo.com',
        caExtensions + '\nextension:nameConstraints:permitted:foo.com', 'ca-nc-perm-foo.com')

    # Intermediate without name constraints issued by constrained root
    writeSpecification('int-ca-nc-perm-foo.com', 'ca-nc-perm-foo.com', caExtensions,
        'int-ca-nc-perm-foo.com')
    generateCommonEndEntityCertificates('int-ca-nc-perm-foo.com', 'int-ca-nc-perm-foo.com')

generateCerts()
