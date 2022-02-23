.. _mozilla_projects_nss_tools_nss_tools_crlutil:

NSS Tools crlutil
=================

.. _using_the_certificate_revocation_list_management_tool:

`Using the Certificate Revocation List Management Tool <#using_the_certificate_revocation_list_management_tool>`__
------------------------------------------------------------------------------------------------------------------

.. container::

   Newsgroup: `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__

   The Certificate Revocation List (CRL) Management Tool is a command-line utility that can list,
   generate, modify, or delete CRLs within the NSS security database file(s) and list, create,
   modify or delete certificates entries in a particular CRL.

   The key and certificate management process generally begins with creating keys in the key
   database, then generating and managing certificates in the certificate database(see ``certutil``
   tool) and continues with certificates expiration or revocation.

   This document discusses certificate revocation list management. For information on security
   module database management, see `Using the Security Module Database Tool <NSS_Tools_modutil>`__.
   For information on certificate and key database management, see `Using the Certificate Database
   Tool <NSS_Tools_certutil>`__.

.. _availability_2:

` <#availability_2>`__ Availability
-----------------------------------

.. container::

   See the :ref:`mozilla_projects_nss_releases` for the platforms this tool is available on.

.. _syntax_2:

` <#syntax_2>`__ Syntax
-----------------------

.. container::

   To run the Certificate Revocation List Management Tool, type the command

   ``crlutil`` *option*\ ``[``\ *arguments*\ ``]``

   where *options* and *arguments* are combinations of the options and arguments listed in the
   following section. Each command takes one option. Each option may take zero or more arguments. To
   see a usage string, issue the command without options, or with the ``-H`` option.

.. _options_and_arguments:

`Options and Arguments <#options_and_arguments>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Options specify an action and are uppercase. Option arguments modify an action and are lowercase.
   Certificate Revocation List Management Tool command options and their arguments are defined as
   follows:

   +-------------------------------------------------+-------------------------------------------------+
   | **Options**                                     |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-G``                                          | Create new Certificate Revocation List(CRL).    |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-D``                                          | Delete Certificate Revocation List from cert    |
   |                                                 | database.                                       |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-I``                                          | Import a CRL to the cert database               |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-E``                                          | Erase all CRLs of specified type from the cert  |
   |                                                 | database                                        |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-L``                                          | List existing CRL located in cert database      |
   |                                                 | file.                                           |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-M``                                          | Modify existing CRL which can be located in     |
   |                                                 | cert db or in arbitrary file. If located in     |
   |                                                 | file it should be encoded in ASN.1 encode       |
   |                                                 | format.                                         |
   +-------------------------------------------------+-------------------------------------------------+
   | **Arguments**                                   |                                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-B``                                          | Bypass CA signature checks.                     |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-P``\ *dbprefix*                              | Specify the prefix used on the                  |
   |                                                 | ``NSS security database`` files (for example,   |
   |                                                 | ``my_cert8.db`` and ``my_key3.db``). This       |
   |                                                 | option is provided as a special case. Changing  |
   |                                                 | the names of the certificate and key databases  |
   |                                                 | is not recommended.                             |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-a``                                          | Use ASCII format or allow the use of ASCII      |
   |                                                 | format for input and output. This formatting    |
   |                                                 | follows `RFC                                    |
   |                                                 | #1113 <http                                     |
   |                                                 | ://andrew2.andrew.cmu.edu/rfc/rfc1113.html>`__. |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-c``\ *crl-gen-file*                          | Specify script file that will be used to        |
   |                                                 | control crl generation/modification. See        |
   |                                                 | crl-cript-file `format <#10232455>`__ below. If |
   |                                                 | options *-M|-G* is used and *-c                 |
   |                                                 | crl-script-file* is not specified, crlutil will |
   |                                                 | read script data from standard input.           |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-d``\ *directory*                             | Specify the database directory containing the   |
   |                                                 | certificate and key database files. On Unix the |
   |                                                 | Certificate Database Tool defaults to           |
   |                                                 | ``$HOME/.netscape`` (that is, ``~/.netscape``). |
   |                                                 | On Windows NT the default is the current        |
   |                                                 | directory.                                      |
   |                                                 |                                                 |
   |                                                 | The ``NSS database`` files must reside in the   |
   |                                                 | same directory.                                 |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-i``\ *crl-import-file*                       | Specify the file which contains the CRL to      |
   |                                                 | import                                          |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-f``\ *password-file*                         | Specify a file that will automatically supply   |
   |                                                 | the password to include in a certificate or to  |
   |                                                 | access a certificate database. This is a        |
   |                                                 | plain-text file containing one password. Be     |
   |                                                 | sure to prevent unauthorized access to this     |
   |                                                 | file.                                           |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-l``\ *algorithm-name*                        | Specify a specific signature algorithm. List of |
   |                                                 | possible algorithms: MD2 \| MD4 \| MD5 \| SHA1  |
   |                                                 | \| SHA256 \| SHA384 \| SHA512                   |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-n``\ *nickname*                              | Specify the nickname of a certificate or key to |
   |                                                 | list, create, add to a database, modify, or     |
   |                                                 | validate. Bracket the *nickname* string with    |
   |                                                 | quotation marks if it contains spaces.          |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-o``\ *output-file*                           | Specify the output file name for new CRL.       |
   |                                                 | Bracket the *output-file* string with quotation |
   |                                                 | marks if it contains spaces. If this argument   |
   |                                                 | is not used the output destination defaults to  |
   |                                                 | standard output.                                |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-t``\ *crl-type*                              | Specify type of CRL. possible types are: 0 -    |
   |                                                 | SEC_KRL_TYPE, 1 - SEC_CRL_TYPE. **This option   |
   |                                                 | is obsolete**                                   |
   +-------------------------------------------------+-------------------------------------------------+
   | ``-u``\ *url*                                   | Specify the url.                                |
   +-------------------------------------------------+-------------------------------------------------+

   +---+
   |   |
   +---+

.. _crl_generation_script_syntax:

`CRL Generation script syntax: <#crl_generation_script_syntax>`__
-----------------------------------------------------------------

.. container::

   CRL generation script file has the following syntax:

   -  Line with comments should have <bold>\ *#*\ </bold> as a first symbol of a line

   -  Set *"this update"* or *"next update"* CRL fields:

         ``update=YYYYMMDDhhmmssZ``
         ``nextupdate=YYYYMMDDhhmmssZ``

      | Field "next update" is optional. Time should be in *GeneralizedTime* format
        (YYYYMMDDhhmmssZ).
      | For example: ``20050204153000Z``

   -  Add an extension to a CRL or a crl certificate entry:

         ``addext``\ *extension-name* *critical/non-critical*\ ``[``\ *arg1*\ ``[``\ *arg2*
         ``...]]``

      | Where:

         ``extension-name``: string value of a name of known extensions.
         ``critical/non-critical``: is 1 when extension is critical and 0 otherwise.
         ``arg1, arg2``: specific to extension type extension parameters

      ``addext`` uses the range that was set earlier by ``addcert`` and will install an extension to
      every cert entries within the range.

      See `"Implemented extensions" <#3543811>`__ for more information regarding extensions and
      theirs parameters.

   -  Add certificate entries(s) to CRL:

         ``addcert``\ *range* *date*

      | Where:

         ``range``: two integer values separated by ``dash``: range of certificates that will be
         added by this command. ``dash`` is used as a delimiter. Only one cert will be added if
         there is no delimiter.
         ``date``: revocation date of a cert. Date should be represented in GeneralizedTime format
         (YYYYMMDDhhmmssZ).

   -  Remove certificate entry(s) from CRL

         ``rmcert`` *range*

      | Where:

         ``range``: two integer values separated by ``dash``: range of certificates that will be
         added by this command. ``dash`` is used as a delimiter. Only one cert will be added if
         there is no delimiter.

   -  Change range of certificate entry(s) in CRL

         ``range`` *new-range*

      | Where:

         ``new-range``: two integer values separated by ``dash``: range of certificates that will be
         added by this command. ``dash`` is used as a delimiter. Only one cert will be added if
         there is no delimiter.

.. _implemented_extensions:

`Implemented Extensions <#implemented_extensions>`__
----------------------------------------------------

.. container::

   The extensions defined for CRL provide methods for associating additional attributes with CRLs of
   theirs entries. For more information see `RFC #3280 <http://www.faqs.org/rfcs/rfc3280.html>`__

   -  Add The Authority Key Identifier extension:

      The authority key identifier extension provides a means of identifying the public key
      corresponding to the private key used to sign a CRL.

         ``authKeyId`` *critical* [*key-id* \| *dn* *cert-serial*]

      | Where:

         ``authKeyIdent``: identifies the name of an extension
         ``critical``: value of 1 of 0. Should be set to 1 if this extension is critical or 0
         otherwise.
         ``key-id``: key identifier represented in octet string. ``dn:``: is a CA distinguished name
         ``cert-serial``: authority certificate serial number.

   -  Add Issuer Alternative Name extension:

      The issuer alternative names extension allows additional identities to be associated with the
      issuer of the CRL. Defined options include an rfc822 name (electronic mail address), a DNS
      name, an IP address, and a URI.

         ``issuerAltNames`` *non-critical* *name-list*

      | Where:

         ``subjAltNames``: identifies the name of an extension
         should be set to 0 since this is non-critical extension
         ``name-list``: comma separated list of names

   -  Add CRL Number extension:

      The CRL number is a non-critical CRL extension which conveys a monotonically increasing
      sequence number for a given CRL scope and CRL issuer. This extension allows users to easily
      determine when a particular CRL supersedes another CRL

         ``crlNumber`` *non-critical* *number*

      | Where:

         ``crlNumber``: identifies the name of an extension
         ``critical``: should be set to 0 since this is non-critical extension
         ``number``: value of ``long`` which identifies the sequential number of a CRL.

   -  Add Revocation Reason Code extension:

      The reasonCode is a non-critical CRL entry extension that identifies the reason for the
      certificate revocation.

         ``reasonCode`` *non-critical* *code*

      | Where:

         | ``reasonCode``: identifies the name of an extension
         | ``non-critical``: should be set to 0 since this is non-critical extension
         | ``code``: the following codes are available:

            unspecified (0),
            keyCompromise (1),
            cACompromise (2),
            affiliationChanged (3),
            superseded (4),
            cessationOfOperation (5),
            certificateHold (6),
            removeFromCRL (8),
            privilegeWithdrawn (9),
            aACompromise (10)

   -  Add Invalidity Date extension:

      The invalidity date is a non-critical CRL entry extension that provides the date on which it
      is known or suspected that the private key was compromised or that the certificate otherwise
      became invalid.

         invalidityDate *non-critical* *date*

      | Where:

         ``crlNumber``: identifies the name of an extension
         ``non-critical``: should be set to 0 since this is non-critical extension ``date``:
         invalidity date of a cert. Date should be represented in GeneralizedTime format
         (YYYYMMDDhhmmssZ).

.. _usage_2:

` <#usage_2>`__ Usage
---------------------

.. container::

   The Certificate Revocation List Management Tool's capabilities are grouped as follows, using
   these combinations of options and arguments. Options and arguments in square brackets are
   optional, those without square brackets are required.

      ``-G|-M -c crl-gen-file -n nickname [-i``\ *crl*\ ``] [-u``\ *url*\ ``] [-d``\ *keydir*\ ``] [-P``\ *dbprefix*\ ``] [-l``\ *alg*\ ``] [-a] [-B]``

   ..

      ``-L [-n``\ *crl-name*\ ``] [-d``\ *krydir*\ ``]``

      ``crlutil -D -n nickname [-d``\ *keydir*\ ``] [-P``\ *dbprefix*\ ``]``

   ..

      ``crlutil -E [-d``\ *keydir*\ ``] [-P``\ *dbprefix*\ ``]``

      ``crlutil -I -i crl [-t``\ *crlType*\ ``] [-u``\ *url*\ ``] [-d``\ *keydir*\ ``] [-P``\ *dbprefix*\ ``] [-B]``

   -  Creating or modifying a CRL:
   -  Listing all CRls or a named CRL:
   -  Deleting CRL from db:
   -  Erasing CRLs from db:
   -  Import CRL from file:

.. _examples_2:

` <#examples_2>`__ Examples
---------------------------

.. container::

   |  `Creating a New CRL <NSS_Tools_certutil#1028724>`__
   | `Listing CRLs in a Database <NSS_Tools_certutil#1034026>`__
   | `Deleting CRL from a Database <NSS_Tools_certutil#1034026>`__
   | `Importing CRL into a Database <NSS_Tools_certutil#1034026>`__
   | `Modifiying CRL in a Database <NSS_Tools_certutil#1034026>`__

.. _creating_a_new_crl:

`Creating a New CRL <#creating_a_new_crl>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example creates a new CRL and importing it in to a Database in the specified directory:

   ``crlutil -G -d``\ *certdir*\ ``-n``\ *cert-nickname*\ ``-c``\ *crl-script-file*

   or

   ``crlutil -G -d``\ *certdir*\ ``-n``\ *cert-nickname*\ ``<<EOF   update=20050204153000Z   addcert 34-40 20050104153000Z   EOF``

   Where *cert-nickname* is the name the new CRL will be signed with.

.. _listing_crls_in_a_database:

`Listing CRLs in a Database <#listing_crls_in_a_database>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example lists all the CRLs in the ``NSS database`` in the specified directory:

   ``crlutil -L -d``\ *certdir*

   The CRL Management Tool displays output similar to the following:

   ``CRL Name              CRL Type``

   ``CN=NSS Test CA,O=BOGUS NSS,L=Mountain View,ST=California,C=US  CRL   CN=John Smith,O=Netscape,L=Mountain View,ST=California,C=US  CRL``

   | To view a particular CRL user should specify *-n nickname* parameter.
   | ``crlutil -L -d``\ *certdir*\ ``-n`` *nickname*

   ``CRL Info:   :       Version: 2 (0x1)       Signature Algorithm: PKCS #1 MD5 With RSA Encryption       Issuer: "CN=NSS Test CA,O=BOGUS NSS,L=Mountain View,ST=California,C=US"       This Update: Wed Feb 23 12:08:38 2005       Entry (1):           Serial Number: 40 (0x28)           Revocation Date: Wed Feb 23 12:08:10 2005       Entry (2):           Serial Number: 42 (0x2a)           Revocation Date: Wed Feb 23 12:08:40 2005``

.. _deleting_crl_from_a_database:

`Deleting CRL from a Database <#deleting_crl_from_a_database>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example deletes CRL from a database in the specified directory:

   ``crlutil -D -n``\ *nickname*\ ``-d``\ *certdir*

.. _importing_crl_into_a_database:

`Importing CRL into a Database <#importing_crl_into_a_database>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example imports CRL into a database:

   ``crlutil -I -i``\ *crl-file*\ ``-d``\ *certdir*

   File should has binary format of ASN.1 encoded CRL data.

.. _modifying_crl_in_a_database:

`Modifying CRL in a Database <#modifying_crl_in_a_database>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   This example modifies a new CRL and importing it in to a Database in the specified directory:

   ``crlutil -G -d``\ *certdir*\ ``-n``\ *cert-nickname*\ ``-c``\ *crl-script-file*

   or

   ``crlutil -M -d``\ *certdir*\ ``-n``\ *cert-nickname*\ ``<<EOF   update=20050204153000Z   addcert 40-60 20050105153000Z   EOF``

   The CRL Management Tool extracts existing CRL from a database, will modify and sign with
   certificate *cert-nickname* and will store it in database. To modify while importing CRL from
   file user should supply ``-i``\ *import-crl-file* option.

   --------------