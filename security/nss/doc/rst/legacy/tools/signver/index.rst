.. _mozilla_projects_nss_tools_signver:

NSS tools : signver
===================

.. container::

   | Name
   |    signver — Verify a detached PKCS#7 signature for a file.
   | Synopsis
   |    signtool -A \| -V -d directory [-a] [-i input_file] [-o output_file] [-s
   |    signature_file] [-v]
   | Description
   |    The Signature Verification Tool, signver, is a simple command-line utility
   |    that unpacks a base-64-encoded PKCS#7 signed object and verifies the
   |    digital signature using standard cryptographic techniques. The Signature
   |    Verification Tool can also display the contents of the signed object.
   | Options
   |    -A
   |            Displays all of the information in the PKCS#7 signature.
   |    -V
   |            Verifies the digital signature.
   |    -d [sql:]directory
   |            Specify the database directory which contains the certificates and
   |            keys.
   |            signver supports two types of databases: the legacy security
   |            databases (cert8.db, key3.db, and secmod.db) and new SQLite
   |            databases (cert9.db, key4.db, and pkcs11.txt). If the prefix sql:
   |            is not used, then the tool assumes that the given databases are in
   |            the old format.
   |    -a
   |            Sets that the given signature file is in ASCII format.
   |    -i input_file
   |            Gives the input file for the object with signed data.
   |    -o output_file
   |            Gives the output file to which to write the results.
   |    -s signature_file
   |            Gives the input file for the digital signature.
   |    -v
   |            Enables verbose output.
   | Extended Examples
   |   Verifying a Signature
   |    The -V option verifies that the signature in a given signature file is
   |    valid when used to sign the given object (from the input file).
   |  signver -V -s signature_file -i signed_file -d sql:/home/my/sharednssdb
   |  signatureValid=yes
   |   Printing Signature Data
   |    The -A option prints all of the information contained in a signature file.
   |    Using the -o option prints the signature file information to the given
   |    output file rather than stdout.
   |  signver -A -s signature_file -o output_file
   | NSS Database Types
   |    NSS originally used BerkeleyDB databases to store security information.
   |    The last versions of these legacy databases are:
   |      o cert8.db for certificates
   |      o key3.db for keys
   |      o secmod.db for PKCS #11 module information
   |    BerkeleyDB has performance limitations, though, which prevent it from
   |    being easily used by multiple applications simultaneously. NSS has some
   |    flexibility that allows applications to use their own, independent
   |    database engine while keeping a shared database and working around the
   |    access issues. Still, NSS requires more flexibility to provide a truly
   |    shared security database.
   |    In 2009, NSS introduced a new set of databases that are SQLite databases
   |    rather than BerkleyDB. These new databases provide more accessibility and
   |    performance:
   |      o cert9.db for certificates
   |      o key4.db for keys
   |      o pkcs11.txt, which is listing of all of the PKCS #11 modules contained
   |        in a new subdirectory in the security databases directory
   |    Because the SQLite databases are designed to be shared, these are the
   |    shared database type. The shared database type is preferred; the legacy
   |    format is included for backward compatibility.
   |    By default, the tools (certutil, pk12util, modutil) assume that the given
   |    security databases follow the more common legacy type. Using the SQLite
   |    databases must be manually specified by using the sql: prefix with the
   |    given security directory. For example:
   |  # signver -A -s signature -d sql:/home/my/sharednssdb
   |    To set the shared database type as the default type for the tools, set the
   |    NSS_DEFAULT_DB_TYPE environment variable to sql:
   |  export NSS_DEFAULT_DB_TYPE="sql"
   |    This line can be set added to the ~/.bashrc file to make the change
   |    permanent.
   |    Most applications do not use the shared database by default, but they can
   |    be configured to use them. For example, this how-to article covers how to
   |    configure Firefox and Thunderbird to use the new shared NSS databases:
   |      o https://wiki.mozilla.org/NSS_Shared_DB_Howto
   |    For an engineering draft on the changes in the shared NSS databases, see
   |    the NSS project wiki:
   |      o https://wiki.mozilla.org/NSS_Shared_DB
   | See Also
   |    signtool (1)
   |    The NSS wiki has information on the new database design and how to
   |    configure applications to use it.
   |      o Setting up the shared NSS database
   |        https://wiki.mozilla.org/NSS_Shared_DB_Howto
   |      o Engineering and technical information about the shared NSS database
   |        https://wiki.mozilla.org/NSS_Shared_DB
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