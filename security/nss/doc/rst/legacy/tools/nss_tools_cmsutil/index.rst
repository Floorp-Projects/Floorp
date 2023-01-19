.. _mozilla_projects_nss_tools_nss_tools_cmsutil:

NSS Tools cmsutil
=================

.. _using_cmsutil:

`Using cmsutil <#using_cmsutil>`__
----------------------------------

.. container::

   Newsgroup: `mozilla.dev.tech.crypto <news://news.mozilla.org/mozilla.dev.tech.crypto>`__
   The cmsutil command-line utility uses the `S/MIME Toolkit <../smime/>`__ to perform basic
   operations, such as encryption and decryption, on `Cryptographic Message
   Syntax (CMS) <http://www.ietf.org/rfc/rfc2630.txt>`__ messages.

.. _syntax_2:

` <#syntax_2>`__ Syntax
-----------------------

.. container::

   To run cmsutil, type the command ``cmsutil``\ *option*\ ``[``\ *arguments*\ ``]`` where *option*
   and *arguments* are combinations of the options and arguments listed in the following section.
   Each command takes one option. Each option may take zero or more arguments. To see a usage
   string, issue the command without options.

.. _options_and_arguments:

`Options and Arguments <#options_and_arguments>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Options specify an action. Option arguments modify an action. The options and arguments for the
   ``cmsutil`` command are defined as follows:

   +------------------------------------------------+------------------------------------------------+
   | **Options**                                    |                                                |
   +------------------------------------------------+------------------------------------------------+
   | ``-D``                                         | Decode a message.                              |
   +------------------------------------------------+------------------------------------------------+
   | ``-C``                                         | Encrypt a message.                             |
   +------------------------------------------------+------------------------------------------------+
   | ``-E``                                         | Envelope a message.                            |
   +------------------------------------------------+------------------------------------------------+
   | ``-O``                                         | Create a certificates-only message.            |
   +------------------------------------------------+------------------------------------------------+
   | ``-S``                                         | Sign a message.                                |
   +------------------------------------------------+------------------------------------------------+
   | **Arguments**                                  |                                                |
   +------------------------------------------------+------------------------------------------------+
   | ``-c`` *content*                               | Use this detached content (decode only).       |
   +------------------------------------------------+------------------------------------------------+
   | ``-d`` *dbdir*                                 | Specify the key/certificate database directory |
   |                                                | (default is ".")                               |
   +------------------------------------------------+------------------------------------------------+
   | ``-e`` *envfile*                               | Specify a file containing an enveloped message |
   |                                                | for a set of recipients to which you would     |
   |                                                | like to send an encrypted message. If this is  |
   |                                                | the first encrypted message for that set of    |
   |                                                | recipients, a new enveloped message will be    |
   |                                                | created that you can then use for future       |
   |                                                | messages (encrypt only).                       |
   +------------------------------------------------+------------------------------------------------+
   | ``-G``                                         | Include a signing time attribute (sign only).  |
   +------------------------------------------------+------------------------------------------------+
   | ``-h`` *num*                                   | Generate email headers with info about CMS     |
   |                                                | message (decode only).                         |
   +------------------------------------------------+------------------------------------------------+
   | ``-i`` *infile*                                | Use *infile* as a source of data (default is   |
   |                                                | stdin).                                        |
   +------------------------------------------------+------------------------------------------------+
   | ``-N`` *nickname*                              | Specify nickname of certificate to sign with   |
   |                                                | (sign only).                                   |
   +------------------------------------------------+------------------------------------------------+
   | ``-n``                                         | Suppress output of contents (decode only).     |
   +------------------------------------------------+------------------------------------------------+
   | ``-o`` *outfile*                               | Use outfile as a destination of data (default  |
   |                                                | is stdout).                                    |
   +------------------------------------------------+------------------------------------------------+
   | ``-P``                                         | Include an S/MIME capabilities attribute.      |
   +------------------------------------------------+------------------------------------------------+
   | -p *password*                                  | Use password as key database password.         |
   +------------------------------------------------+------------------------------------------------+
   | -                                              | Specify list of recipients (email addresses)   |
   | r&nbsp\ *recipient1*,\ *recipient2, . .&nbsp.* | for an encrypted or enveloped message. For     |
   |                                                | certificates-only message, list of             |
   |                                                | certificates to send.                          |
   +------------------------------------------------+------------------------------------------------+
   | -T                                             | Suppress content in CMS message (sign only).   |
   +------------------------------------------------+------------------------------------------------+
   | -u *certusage*                                 | Set type of cert usage (default is             |
   |                                                | <tt>certUsageEmailSigner)</tt>.                |
   +------------------------------------------------+------------------------------------------------+
   | <-Y *ekprefnick*                               | Specify an encryption key preference by        |
   |                                                | nickname.                                      |
   +------------------------------------------------+------------------------------------------------+

`Usage <#usage>`__
------------------

.. container::

   cmsutil -C [-i *infile*] [-o *outfile*] [-d *dbdir*] [-p *password*] -r
   "*recipient1*,\ *recipient2*, . . ." -e *envfile*
   cmsutil -D [-i *infile*] [-o *outfile*] [-d *dbdir*] [-p *password*] [-c *content*] [-n] [-h
   *num*]

   cmsutil -E [-i *infile*] [-o *outfile*] [-d *dbdir*] [-p *password*] -r
   "*recipient1*,\ *recipient2*,&nbsp.&nbsp.&nbsp."

   cmsutil -O [-i *infile*] [-o *outfile*] [-d *dbdir*] [-p *password*] -r
   "*cert1*,\ *cert2*, . . ."

   cmsutil -S [-i *infile*] [-o *outfile*] [-d *dbdir*] [-p *password*] -N *nickname*\ [-TGP] [-Y
   *ekprefnick*]