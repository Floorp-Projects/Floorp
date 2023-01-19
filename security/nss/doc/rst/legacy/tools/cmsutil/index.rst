.. _mozilla_projects_nss_tools_cmsutil:

NSS tools : cmsutil
===================

.. container::

   | Name
   |    cmsutil — Performs basic cryptograpic operations, such as encryption and
   |    decryption, on Cryptographic Message Syntax (CMS) messages.
   | Synopsis
   |    cmsutil [options] `arguments <arguments>`__
   | Description
   |    The cmsutil command-line uses the S/MIME Toolkit to perform basic
   |    operations, such as encryption and decryption, on Cryptographic Message
   |    Syntax (CMS) messages.
   |    To run cmsutil, type the command cmsutil option [arguments] where option
   |    and arguments are combinations of the options and arguments listed in the
   |    following section. Each command takes one option. Each option may take
   |    zero or more arguments. To see a usage string, issue the command without
   |    options.
   | Options and Arguments
   |    Options
   |    Options specify an action. Option arguments modify an action. The options
   |    and arguments for the cmsutil command are defined as follows:
   |    -D
   |            Decode a message.
   |    -C
   |            Encrypt a message.
   |    -E
   |            Envelope a message.
   |    -O
   |            Create a certificates-only message.
   |    -S
   |            Sign a message.
   |    Arguments
   |    Option arguments modify an action and are lowercase.
   |    -c content
   |            Use this detached content (decode only).
   |    -d dbdir
   |            Specify the key/certificate database directory (default is ".")
   |    -e envfile
   |            Specify a file containing an enveloped message for a set of
   |            recipients to which you would like to send an encrypted message.
   |            If this is the first encrypted message for that set of recipients,
   |            a new enveloped message will be created that you can then use for
   |            future messages (encrypt only).
   |    -G
   |            Include a signing time attribute (sign only).
   |    -h num
   |            Generate email headers with info about CMS message (decode only).
   |    -i infile
   |            Use infile as a source of data (default is stdin).
   |    -N nickname
   |            Specify nickname of certificate to sign with (sign only).
   |    -n
   |            Suppress output of contents (decode only).
   |    -o outfile
   |            Use outfile as a destination of data (default is stdout).
   |    -P
   |            Include an S/MIME capabilities attribute.
   |    -p password
   |            Use password as key database password.
   |    -r recipient1,recipient2, ...
   |            Specify list of recipients (email addresses) for an encrypted or
   |            enveloped message. For certificates-only message, list of
   |            certificates to send.
   |    -T
   |            Suppress content in CMS message (sign only).
   |    -u certusage
   |            Set type of cert usage (default is certUsageEmailSigner).
   |    -Y ekprefnick
   |            Specify an encryption key preference by nickname.
   | Usage
   |    Encrypt Example
   |  cmsutil -C [-i infile] [-o outfile] [-d dbdir] [-p password] -r "recipient1,recipient2, . . ."
     -e envfile
   |    Decode Example
   |  cmsutil -D [-i infile] [-o outfile] [-d dbdir] [-p password] [-c content] [-n] [-h num]
   |    Envelope Example
   |  cmsutil -E [-i infile] [-o outfile] [-d dbdir] [-p password] -r "recipient1,recipient2, ..."
   |    Certificate-only Example
   |  cmsutil -O [-i infile] [-o outfile] [-d dbdir] [-p password] -r "cert1,cert2, . . ."
   |    Sign Message Example
   |  cmsutil -S [-i infile] [-o outfile] [-d dbdir] [-p password] -N nickname[-TGP] [-Y ekprefnick]
   | See also
   |    certutil(1)
   | See Also
   | Additional Resources
   |    NSS is maintained in conjunction with PKI and security-related projects
   |    through Mozilla dn Fedora. The most closely-related project is Dogtag PKI,
   |    with a project wiki at [1]\ http://pki.fedoraproject.org/wiki/.
   |    For information specifically about NSS, the NSS project wiki is located at
   |   
     [2]\ `http://www.mozilla.org/projects/security/pki/nss/ <https://www.mozilla.org/projects/security/pki/nss/>`__.
     The NSS site relates
   |    directly to NSS code changes and releases.
   |    Mailing lists: pki-devel@redhat.com and pki-users@redhat.com
   |    IRC: Freenode at #dogtag-pki
   | Authors
   |    The NSS tools were written and maintained by developers with Netscape and
   |    now with Red Hat.
   |    Authors: Elio Maldonado <emaldona@redhat.com>, Deon Lackey
   |    <dlackey@redhat.com>.
   | Copyright
   |    (c) 2010, Red Hat, Inc. Licensed under the GNU Public License version 2.
   | References
   |    Visible links
   |    1. http://pki.fedoraproject.org/wiki/
   |    2.
     `http://www.mozilla.org/projects/security/pki/nss/ <https://www.mozilla.org/projects/security/pki/nss/>`__