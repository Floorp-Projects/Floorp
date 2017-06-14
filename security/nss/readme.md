# Network Security Services

Network Security Services (NSS) is a set of libraries designed to support
cross-platform development of security-enabled client and server
applications. NSS supports SSL v3-TLS 1.2 (experimental TLS 1.3), PKCS #5, PKCS#7,
PKCS #11, PKCS #12, S/MIME, X.509 v3 certificates, and other security
standards.

## Getting started

In order to get started create a new directory on that you will be uses as your
local work area, and check out NSS and NSPR. (Note that there's no git mirror of
NSPR and you require mercurial to get the latest NSPR source.)

    git clone https://github.com/nss-dev/nss.git
    hg clone https://hg.mozilla.org/projects/nspr

NSS can also be cloned with mercurial

    hg clone https://hg.mozilla.org/projects/nss

## Building NSS

**This build system is under development. It does not yet support all the
features or platforms that NSS supports. To build on anything other than Mac or
Linux please use the legacy build system as described below.**

Build requirements:

* [gyp](https://gyp.gsrc.io/)
* [ninja](https://ninja-build.org/)

After changing into the NSS directory a typical build is done as follows

    ./build.sh

Once the build is done the build output is found in the directory
`../dist/Debug` for debug builds and `../dist/Release` for opt builds.
Exported header files can be found in the `include` directory, library files in
directory `lib`, and tools in directory `bin`. In order to run the tools, set
your system environment to use the libraries of your build from the "lib"
directory, e.g., using the `LD_LIBRARY_PATH` or `DYLD_LIBRARY_PATH`.

    Usage: build.sh [-hcv] [-j <n>] [--nspr] [--gyp|-g] [--opt|-o] [-m32]
                    [--test] [--pprof] [--scan-build[=output]] [--ct-verif]
                    [--asan] [--ubsan] [--msan] [--sancov[=edge|bb|func|...]]
                    [--disable-tests] [--fuzz[=tls|oss]] [--system-sqlite]
                    [--no-zdefs] [--with-nspr] [--system-nspr] [--enable-libpkix]

    This script builds NSS with gyp and ninja.

    This build system is still under development.  It does not yet support all
    the features or platforms that NSS supports.

    NSS build tool options:

        -h               display this help and exit
        -c               clean before build
        -v               verbose build
        -j <n>           run at most <n> concurrent jobs
        --nspr           force a rebuild of NSPR
        --gyp|-g         force a rerun of gyp
        --opt|-o         do an opt build
        -m32             do a 32-bit build on a 64-bit system
        --test           ignore map files and export everything we have
        --fuzz           build fuzzing targets (this always enables test builds)
                         --fuzz=tls to enable TLS fuzzing mode
                         --fuzz=oss to build for OSS-Fuzz
        --pprof          build with gperftool support
        --ct-verif       build with valgrind for ct-verif
        --scan-build     run the build with scan-build (scan-build has to be in the path)
                         --scan-build=/out/path sets the output path for scan-build
        --asan           do an asan build
        --ubsan          do an ubsan build
                         --ubsan=bool,shift,... sets specific UB sanitizers
        --msan           do an msan build
        --sancov         do sanitize coverage builds
                         --sancov=func sets coverage to function level for example
        --disable-tests  don't build tests and corresponding cmdline utils
        --system-sqlite  use system sqlite
        --no-zdefs       don't set -Wl,-z,defs
        --with-nspr      don't build NSPR but use the one at the given location, e.g.
                         --with-nspr=/path/to/nspr/include:/path/to/nspr/lib
        --system-nspr    use system nspr. This requires an installation of NSPR and
                         might not work on all systems.
        --enable-libpkix make libpkix part of the build.

## Building NSS (legacy build system)

After changing into the NSS directory a typical build of 32-bit NSS is done as
follows:

    make nss_build_all

The following environment variables might be useful:

* `BUILD_OPT=1` to get an optimised build

* `USE_64=1` to get a 64-bit build (recommended)

The complete list of environment variables can be found
[here](https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSS/Reference/NSS_environment_variables).

To clean the build directory run:

    make nss_clean_all

## Tests

### Setup

Make sure that the address `$HOST.$DOMSUF` on your computer is available. This
is necessary because NSS tests generate certificates and establish TLS
connections, which requires a fully qualified domain name.
You can test this by
calling `ping $HOST.$DOMSUF`. If this is working, you're all set.  If it's not,
set or export:

    HOST=nss
    DOMSUF=local

Note that you might have to add `nss.local` to `/etc/hosts` if it's not
there. The entry should look something like `127.0.0.1 nss.local nss`.

If you get name resolution errors, try to ensure that you are using an IPv4
address; IPv6 is the default on many systems for the loopback device which
doesn't work.

### Running tests

**Runnning all tests will take a while!**

    cd tests
    ./all.sh

Make sure that all environment variables set for the build are set while running
the tests as well.  Test results are published in the folder
`../../test_results/`.

Individual tests can be run with the `NSS_TESTS` environment variable,
e.g. `NSS_TESTS=ssl_gtests ./all.sh` or by changing into the according directory
and running the bash script there `cd ssl_gtests && ./ssl_gtests.sh`.  The
following tests are available:

    cipher lowhash libpkix cert dbtests tools fips sdr crmf smime ssl ocsp merge pkits chains ec gtests ssl_gtests bogo

To make tests run faster it's recommended to set `NSS_CYCLES=standard` to run
only the standard cycle.

## Releases

NSS releases can be found at [Mozilla's download
server](https://ftp.mozilla.org/pub/security/nss/releases/). Because NSS depends
on the base library NSPR you should download the archive that combines both NSS
and NSPR.

## Contributing

[Bugzilla](https://bugzilla.mozilla.org/) is used to track NSS development and
bugs. File new bugs in the NSS product.

A list with good first bugs to start with are [listed
here](https://bugzilla.mozilla.org/buglist.cgi?keywords=good-first-bug%2C%20&keywords_type=allwords&list_id=13238861&resolution=---&query_format=advanced&product=NSS).

### NSS Folder Structure

The nss directory contains the following important subdirectories:

- `coreconf` contains the build logic.

- `lib` contains all library code that is used to create the runtime libraries.

- `cmd` contains a set of various tool programs that are built with NSS. Several
  tools are general purpose and can be used to inspect and manipulate the
  storage files that software using the NSS library creates and modifies. Other
  tools are only used for testing purposes.

- `test` and `gtests` contain the NSS test suite. While `test` contains shell
  scripts to drive test programs in `cmd`, `gtests` holds a set of
  [gtests](https://github.com/google/googletest).

A more comprehensible overview of the NSS folder structure and API guidelines
can be found
[here](https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSS/NSS_API_Guidelines).
