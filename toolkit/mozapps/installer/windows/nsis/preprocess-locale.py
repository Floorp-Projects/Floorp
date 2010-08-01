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
from os.path import join
import sys

def preprocess_locale(argv):
    """
    Validates command line arguments and displays usage if necessary
    """
    if len(argv) < 1 or (argv[0] != '--convert-utf8-utf16le' and argv[0] != '--preprocess-locale'):
        sys.stderr.write("""
preprocess-locale.py

Commands:
 --convert-utf8-utf16le - preprocesses installer locale properties files and
                          creates a basic NSIS nlf file
 --preprocess-locale    - converts a UTF-8 file to a new UTF-16LE file

use "preprocess-locale.py <command>" to see the usage for each command
""")
        sys.exit(1)

    if argv[0] == '--convert-utf8-utf16le':
        if len(argv) != 3:
            sys.stderr.write("""
Converts a UTF-8 file to UTF-16LE

Usage: preprocess-locale.py --convert-utf8-utf16le <src> <dest>

Arguments:
 <src> \tthe path to the UTF-8 source file to convert
 <dest>\tthe path to the UTF-16LE destination file to create
""")
            sys.exit(1)
	convert_utf8_utf16le(argv[1], argv[2])

    if argv[0] == '--preprocess-locale':
        if len(argv) != 5:
            sys.stderr.write("""
Preprocesses the installer localized properties files into the format
required by NSIS and creates a basic NSIS nlf file.

Usage: preprocess-locale.py --preprocess-locale <src> <locale> <code> <dest>

Arguments:
 <src>   \tthe path to top source directory for the toolkit source
 <locale>\tthe path to the installer's locale files
 <code>  \tthe locale code
 <dest>  \tthe path to the destination directory
""")
            sys.exit(1)
        preprocess_locale_files(argv[1], argv[2], argv[3], argv[4])

def open_utf16le_file(path):
    """
    Returns an opened file object with a a UTF-16LE byte order mark.
    """
    fp = open(path, "w+b")
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
    fp = open(path, "r")
    for line in fp:
        line = line.strip()
        if line == "" or line[0] == "#":
            continue

        name, value = line.split("=", 1)
        value = value.strip() # trim whitespace from the start and end
        if value[-1] == "\"" and value[0] == "\"":
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

def preprocess_locale_files(moz_dir, locale_dir, ab_cd, config_dir):
    """
    Preprocesses the installer localized properties files into the format
    required by NSIS and creates a basic NSIS nlf file.

    Parameters:
    moz_dir    - the path to top source directory for the toolkit source
    locale_dir - the path to the installer's locale files
    ab_cd      - the locale code
    config_dir - the path to the destination directory
    """

    # Set the language ID to 0 to make this locale the default locale. An
    # actual ID will need to be used to create a multi-language installer
    # (e.g. for CD distributions, etc.).
    lang_id = "0"
    rtl = "-"

    # Check whether the locale is right to left from locales.nsi.
    fp = open(join(moz_dir, "toolkit/mozapps/installer/windows/nsis/locales.nsi"), "r")
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
%s
# Font and size - dash (-) means default
-
-
# Codepage - dash (-) means ANSI code page
-
# RTL - anything else than RTL means LTR
%s
""" % (lang_id, rtl)).encode("utf-16-le"))
    fp.close()

    # Create the main NSIS language file
    fp = open_utf16le_file(join(config_dir, "overrideLocale.nsh"))
    locale_strings = get_locale_strings(join(locale_dir, "override.properties"),
                                        "LangString ^", " " + lang_id + " ", False)
    fp.write(unicode(locale_strings, "utf-8").encode("utf-16-le"))
    fp.close()

    # Create the Modern User Interface language file
    fp = open_utf16le_file(join(config_dir, "baseLocale.nsh"))
    fp.write((u""";NSIS Modern User Interface - Language File
;Compatible with Modern UI 1.68
;Language: baseLocale (%s)
!insertmacro MOZ_MUI_LANGUAGEFILE_BEGIN \"baseLocale\"
!define MUI_LANGNAME \"baseLocale\"
""" % (lang_id)).encode("utf-16-le"))
    locale_strings = get_locale_strings(join(locale_dir, "mui.properties"),
                                        "!define ", " ", True)
    fp.write(unicode(locale_strings, "utf-8").encode("utf-16-le"))
    fp.write(u"!insertmacro MOZ_MUI_LANGUAGEFILE_END\n".encode("utf-16-le"))
    fp.close()

    # Create the custom language file for our custom strings
    fp = open_utf16le_file(join(config_dir, "customLocale.nsh"))
    locale_strings = get_locale_strings(join(locale_dir, "custom.properties"),
                        "LangString ", " " + lang_id + " ", True)
    fp.write(unicode(locale_strings, "utf-8").encode("utf-16-le"))
    fp.close()

def convert_utf8_utf16le(in_file_path, out_file_path):
    """
    Converts a UTF-8 file to a new UTF-16LE file

    Arguments:
    in_file_path  - the path to the UTF-8 source file to convert
    out_file_path - the path to the UTF-16LE destination file to create
    """
    in_fp = open(in_file_path, "r")
    out_fp = open_utf16le_file(out_file_path)
    out_fp.write(unicode(in_fp.read(), "utf-8").encode("utf-16-le"))
    in_fp.close()
    out_fp.close()

if __name__ == '__main__':
    sys.exit(preprocess_locale(sys.argv[1:]))
