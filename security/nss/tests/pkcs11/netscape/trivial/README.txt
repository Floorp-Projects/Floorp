This is a very trivial program that loads and excercises a PKCS#11
module, trying basic operations.  I used it as a basic check that
my data-only modules for NSS worked, and I'm including it here as
a first sample test program.


This program uses GNU autoconf: run ./configure --help for info.
In addition to the standard options, the configure script accepts
the following:

  --with-nspr[=path]      specify location of NSPR
  --with-nss-dist[=path]  specify path to NSS dist directory
  --with-nss-hdrs[=path]  or, specify path to installed NSS headers
  --with-rsa-hdrs[=path]  if not using NSS, specify path to RSA headers
  --disable-debug         default is enabled

This program uses NSPR; you may specify the path to your NSPR 
installation by using the "--with-nspr" option.  The specified
directory should be the one containing "include" and "lib."
If this option is not given, the default is the usual prefix
directories; see ./configure --help for more info.

This program requires either the pkcs11*.h files from RSA, or
the NSS equivalents.  To specify their location, you must
specify one of --with-nss-dist, --with-nss-hdrs, or --with-rsa-hdrs.

If you have an NSS build tree, specify --with-nss-dist and provide
the path to the mozilla/dist/*.OBJ directory.  (If you got this
package by checking it out from mozilla, it should be about six
directories up, once you've built NSS.)

Alternatively, if you have an NSS installation (including "private"
files, e.g. "ck.h") you may point directly to the directory containing
the headers with --with-nss-hdrs.

If you would rather use the RSA-provided header files, or your own
versions of them, specify their location with --with-rsa-hdrs.

The flag --disable-debug doesn't really do much here other than
exclude the CVS_ID info from the binary.


To run the program, specify the name of the .so (or your platform's
equivalent) containing the module to be tested, e.g.: 

  ./trivial ../../../../../../dist/*.OBJ/lib/libnssckbi.so


If you're using NSS, and using our experimental "installer's
arguments" fields in CK_C_INITIALIZE_ARGS, you can specify an
"installer argument" with the -i flag:

  ./trivial -i ~/.netscape/certs.db [...]/libnssckdb.so


Share and enjoy.
