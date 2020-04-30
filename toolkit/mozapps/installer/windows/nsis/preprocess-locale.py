# preprocess-locale.py
# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

# preprocess-locale.py provides two functions depending on the arguments passed
# to it when invoked.
#
# Preprocesses installer locale properties files and creates a basic NSIS nlf
# file when invoked with --preprocess-locale.
# 
# Converts a UTF-8 file to a new UTF-16LE file when invoked with
# --convert-utf8-utf16le.

from codecs import BOM_UTF16_LE
import io
from os.path import join, isfile
import six
import sys
from optparse import OptionParser

def open_utf16le_file(path):
    """
    Returns an opened file object with a a UTF-16LE byte order mark.
    """
    fp = io.open(path, "w+b")
    fp.write(BOM_UTF16_LE)
    return fp

def get_locale_strings(path, prefix, middle, add_cr):
    """
    Returns a string created by converting an installer locale properties file
    into the format required by NSIS locale files.

    Parameters:
    path   - the path to the installer locale properties file to preprocess
    prefix - a string to prefix each line with
    middle - a string to insert between the name and value for each line
    add_cr - boolean for whether to add an NSIS carriage return before NSIS
             linefeeds when there isn't one already
    """
    output = ""
    fp = io.open(path, "r", encoding="utf-8")
    for line in fp:
        line = line.strip()
        if line == "" or line[0] == "#":
            continue

        name, value = line.split("=", 1)
        value = value.strip() # trim whitespace from the start and end
        if value and value[-1] == "\"" and value[0] == "\"":
            value = value[1:-1] # remove " from the start and end

        if add_cr:
            value = value.replace("\\n", "\\r\\n") # prefix $\n with $\r
            value = value.replace("\\r\\r", "\\r") # replace $\r$\r with $\r

        value = value.replace("\"", "$\\\"") # prefix " with $\
        value = value.replace("\\r", "$\\r") # prefix \r with $
        value = value.replace("\\n", "$\\n") # prefix \n with $
        value = value.replace("\\t", "$\\t") # prefix \t with $

        output += prefix + name.strip() + middle + " \"" + value + "\"\n"
    fp.close()
    return output

def lookup(path, l10ndirs):
    for d in l10ndirs:
        if isfile(join(d, path)):
            return join(d, path)
    return join(l10ndirs[-1], path)

def preprocess_locale_files(config_dir, l10ndirs):
    """
    Preprocesses the installer localized properties files into the format
    required by NSIS and creates a basic NSIS nlf file.

    Parameters:
    config_dir - the path to the destination directory
    l10ndirs   - list of paths to search for installer locale files
    """

    # Create the main NSIS language file
    fp = open_utf16le_file(join(config_dir, "overrideLocale.nsh"))
    locale_strings = get_locale_strings(lookup("override.properties",
                                               l10ndirs),
                                        "LangString ^",
                                        " 0 ",
                                        False)
    fp.write(locale_strings.encode("utf-16-le"))
    fp.close()

    # Create the Modern User Interface language file
    fp = open_utf16le_file(join(config_dir, "baseLocale.nsh"))
    fp.write((u""";NSIS Modern User Interface - Language File
;Compatible with Modern UI 1.68
;Language: baseLocale (0)
!insertmacro MOZ_MUI_LANGUAGEFILE_BEGIN \"baseLocale\"
!define MUI_LANGNAME \"baseLocale\"
""").encode("utf-16-le"))
    locale_strings = get_locale_strings(lookup("mui.properties", l10ndirs),
                                        "!define ", " ", True)
    fp.write(locale_strings.encode("utf-16-le"))
    fp.write(u"!insertmacro MOZ_MUI_LANGUAGEFILE_END\n".encode("utf-16-le"))
    fp.close()

    # Create the custom language file for our custom strings
    fp = open_utf16le_file(join(config_dir, "customLocale.nsh"))
    locale_strings = get_locale_strings(lookup("custom.properties",
                                               l10ndirs),
                                        "LangString ",
                                        " 0 ",
                                        True)
    fp.write(locale_strings.encode("utf-16-le"))
    fp.close()

def create_nlf_file(moz_dir, ab_cd, config_dir):
    """
    Create a basic NSIS nlf file.

    Parameters:
    moz_dir    - the path to top source directory for the toolkit source
    ab_cd      - the locale code
    config_dir - the path to the destination directory
    """
    rtl = "-"

    # Check whether the locale is right to left from locales.nsi.
    fp = io.open(join(moz_dir,
                      "toolkit/mozapps/installer/windows/nsis/locales.nsi"),
                 "r", encoding='utf-8')
    for line in fp:
        line = line.strip()
        if line == "!define " + ab_cd + "_rtl":
            rtl = "RTL"
            break

    fp.close()

    # Create the main NSIS language file with RTL for right to left locales
    # along with the default codepage, font name, and font size represented
    # by the '-' character.
    fp = open_utf16le_file(join(config_dir, "baseLocale.nlf"))
    fp.write((u"""# Header, don't edit
NLF v6
# Start editing here
# Language ID
0
# Font and size - dash (-) means default
-
-
# Codepage - dash (-) means ANSI code page
-
# RTL - anything else than RTL means LTR
%s
""" % rtl).encode("utf-16-le"))
    fp.close()

def preprocess_locale_file(config_dir,
                           l10ndirs,
                           properties_filename,
                           output_filename):
    """
    Preprocesses a single localized properties file into the format
    required by NSIS and creates a basic NSIS nlf file.

    Parameters:
    config_dir            - the path to the destination directory
    l10ndirs              - list of paths to search for installer locale files
    properties_filename   - the name of the properties file to search for
    output_filename       - the output filename to write
    """

    # Create the custom language file for our custom strings
    fp = open_utf16le_file(join(config_dir, output_filename))
    locale_strings = get_locale_strings(lookup(properties_filename,
                                               l10ndirs),
                                        "LangString ",
                                        " 0 ",
                                        True)
    fp.write(locale_strings.encode("utf-16-le"))
    fp.close()


def convert_utf8_utf16le(in_file_path, out_file_path):
    """
    Converts a UTF-8 file to a new UTF-16LE file

    Arguments:
    in_file_path  - the path to the UTF-8 source file to convert
    out_file_path - the path to the UTF-16LE destination file to create
    """
    in_fp = open(in_file_path, "r", encoding='utf-8')
    out_fp = open_utf16le_file(out_file_path)
    out_fp.write(in_fp.read().encode("utf-16-le"))
    in_fp.close()
    out_fp.close()

if __name__ == '__main__':
    usage = """usage: %prog command <args>

Commands:
 --convert-utf8-utf16le     - Preprocesses installer locale properties files
 --preprocess-locale        - Preprocesses the installer localized properties
                              files into the format required by NSIS and
                              creates a basic NSIS nlf file.
 --preprocess-single-file   - Preprocesses a single properties file into the
                              format required by NSIS
 --create-nlf-file          - Creates a basic NSIS nlf file

preprocess-locale.py --preprocess-locale <src> <locale> <code> <dest>

Arguments:
 <src>   \tthe path to top source directory for the toolkit source
 <locale>\tthe path to the installer's locale files
 <code>  \tthe locale code
 <dest>  \tthe path to the destination directory


preprocess-locale.py --preprocess-single-file <src>
                                              <locale>
                                              <dest>
                                              <infile>
                                              <outfile>

Arguments:
 <src>    \tthe path to top source directory for the toolkit source
 <locale> \tthe path to the installer's locale files
 <dest>   \tthe path to the destination directory
 <infile> \tthe properties file to process
 <outfile>\tthe nsh file to write


preprocess-locale.py --create-nlf-file <src>
                                       <code>
                                       <dest>

Arguments:
 <src>    \tthe path to top source directory for the toolkit source
 <code>   \tthe locale code
 <dest>   \tthe path to the destination directory


preprocess-locale.py --convert-utf8-utf16le <src> <dest>

Arguments:
 <src> \tthe path to the UTF-8 source file to convert
 <dest>\tthe path to the UTF-16LE destination file to create
"""

    preprocess_locale_args_help_string = """\
Arguments to --preprocess-locale should be:
   <src> <locale> <code> <dest>
or
   <src> <code> <dest> --l10n-dir <dir> [--l10n-dir <dir> ...]"""

    preprocess_single_file_args_help_string = """\
Arguments to --preprocess-single_file should be:
   <src> <locale> <code> <dest> <infile> <outfile>
or
   <src> <locale> <code> <dest> <infile> <outfile>
   --l10n-dir <dir> [--l10n-dir <dir>...]"""

    create_nlf_args_help_string = """\
Arguments to --create-nlf-file should be:
   <src> <code> <dest>"""

    p = OptionParser(usage=usage)
    p.add_option("--preprocess-locale", action="store_true", default=False,
                 dest='preprocess')
    p.add_option("--preprocess-single-file", action="store_true", default=False,
                 dest='preprocessSingle')
    p.add_option("--create-nlf-file", action="store_true", default=False,
                 dest='createNlf')
    p.add_option("--l10n-dir", action="append", default=[],
                 dest="l10n_dirs",
                 help="Add directory to lookup for locale files")
    p.add_option("--convert-utf8-utf16le", action="store_true", default=False,
                 dest='convert')

    options, args = p.parse_args()

    foundOne = False
    if (options.preprocess):
        foundOne = True
    if (options.convert):
        if(foundOne):
            p.error("More than one command specified")
        else:
            foundOne = True
    if (options.preprocessSingle):
        if(foundOne):
            p.error("More than one command specified")
        else:
            foundOne = True
    if (options.createNlf):
        if(foundOne):
            p.error("More than one command specified")
        else:
            foundOne = True

    if (not foundOne):
      p.error("No command specified")

    if options.preprocess:
        if len(args) not in (3,4):
            p.error(preprocess_locale_args_help_string)

        # Parse args
        pargs = args[:]
        moz_dir = pargs[0]
        if len(pargs) == 4:
            l10n_dirs = [pargs[1]]
            del pargs[1]
        else:
            if not options.l10n_dirs:
                p.error(preprocess_locale_args_help_string)
            l10n_dirs = options.l10n_dirs
        ab_cd = pargs[1]
        config_dir = pargs[2]

        # Create the output files
        create_nlf_file(moz_dir, ab_cd, config_dir)
        preprocess_locale_files(config_dir, l10n_dirs)
    elif options.preprocessSingle:
        if len(args) not in (4,5):
            p.error(preprocess_single_file_args_help_string)

        # Parse args
        pargs = args[:]
        moz_dir = pargs[0]
        if len(pargs) == 5:
            l10n_dirs = [pargs[1]]
            del pargs[1]
        else:
            if not options.l10n_dirs:
                p.error(preprocess_single_file_args_help_string)
            l10n_dirs = options.l10n_dirs
        config_dir = pargs[1]
        in_file = pargs[2]
        out_file = pargs[3]

        # Create the output files
        preprocess_locale_file(config_dir,
                               l10n_dirs,
                               in_file,
                               out_file)
    elif options.createNlf:
        if len(args) != 3:
            p.error(create_nlf_args_help_string)

        # Parse args
        pargs = args[:]
        moz_dir = pargs[0]
        ab_cd = pargs[1]
        config_dir = pargs[2]

        # Create the output files
        create_nlf_file(moz_dir, ab_cd, config_dir)
    elif options.convert:
        if len(args) != 2:
            p.error("--convert-utf8-utf16le needs both of <src> <dest>")
        convert_utf8_utf16le(*args)
