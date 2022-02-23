.. _mozilla_projects_nss_tools_vfychain:

NSS tools : vfychain
====================

.. container::

   | Name
   |    vfychain — vfychain [options] [revocation options] certfile [[options]
   |    certfile] ...
   | Synopsis
   |    vfychain
   | Description
   |    The verification Tool, vfychain, verifies certificate chains. modutil can
   |    add and delete PKCS #11 modules, change passwords on security databases,
   |    set defaults, list module contents, enable or disable slots, enable or
   |    disable FIPS 140-2 compliance, and assign default providers for
   |    cryptographic operations. This tool can also create certificate, key, and
   |    module security database files.
   |    The tasks associated with security module database management are part of
   |    a process that typically also involves managing key databases and
   |    certificate databases.
   | Options
   |    -a
   |            the following certfile is base64 encoded
   |    -b YYMMDDHHMMZ
   |            Validate date (default: now)
   |    -d directory
   |            database directory
   |    -f
   |            Enable cert fetching from AIA URL
   |    -o oid
   |            Set policy OID for cert validation(Format OID.1.2.3)
   |    -p
   |            Use PKIX Library to validate certificate by calling:
   |            \* CERT_VerifyCertificate if specified once,
   |            \* CERT_PKIXVerifyCert if specified twice and more.
   |    -r
   |            Following certfile is raw binary DER (default)
   |    -t
   |            Following cert is explicitly trusted (overrides db trust)
   |    -u usage
   |            0=SSL client, 1=SSL server, 2=SSL StepUp, 3=SSL CA, 4=Email
   |            signer, 5=Email recipient, 6=Object signer,
   |            9=ProtectedObjectSigner, 10=OCSP responder, 11=Any CA
   |    -v
   |            Verbose mode. Prints root cert subject(double the argument for
   |            whole root cert info)
   |    -w password
   |            Database password
   |    -W pwfile
   |            Password file
   |            Revocation options for PKIX API (invoked with -pp options) is a
   |            collection of the following flags: [-g type [-h flags] [-m type
   |            [-s flags]] ...] ...
   |            Where:
   |    -g test-type
   |            Sets status checking test type. Possible values are "leaf" or
   |            "chain"
   |    -g test type
   |            Sets status checking test type. Possible values are "leaf" or
   |            "chain".
   |    -h test flags
   |            Sets revocation flags for the test type it follows. Possible
   |            flags: "testLocalInfoFirst" and "requireFreshInfo".
   |    -m method type
   |            Sets method type for the test type it follows. Possible types are
   |            "crl" and "ocsp".
   |    -s method flags
   |            Sets revocation flags for the method it follows. Possible types
   |            are "doNotUse", "forbidFetching", "ignoreDefaultSrc",
   |            "requireInfo" and "failIfNoInfo".
   | Additional Resources
   |    For information about NSS and other tools related to NSS (like JSS), check
   |    out the NSS project wiki at
   |   
     [1]\ `http://www.mozilla.org/projects/security/pki/nss/ <https://www.mozilla.org/projects/security/pki/nss/>`__.
     The NSS site relates
   |    directly to NSS code changes and releases.
   |    Mailing lists: https://lists.mozilla.org/listinfo/dev-tech-crypto
   |    IRC: Freenode at #dogtag-pki
   | Authors
   |    The NSS tools were written and maintained by developers with Netscape, Red
   |    Hat, and Sun.
   |    Authors: Elio Maldonado <emaldona@redhat.com>, Deon Lackey
   |    <dlackey@redhat.com>.
   | Copyright
   |    (c) 2010, Red Hat, Inc. Licensed under the GNU Public License version 2.
   | References
   |    Visible links
   |    1.
     `http://www.mozilla.org/projects/security/pki/nss/ <https://www.mozilla.org/projects/security/pki/nss/>`__