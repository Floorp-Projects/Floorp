If this is your first time building the HTML5 parser, you need to execute the
following commands (from this directory) to bootstrap the translation:

  make sync             # fetch remote source files and licenses
  make translate        # perform the Java-to-C++ translation from the remote
                        # sources
  make named_characters # Generate tables for named character tokenization

If you make changes to the translator or the javaparser, you can rebuild by
retyping 'make' in this directory.  If you make changes to the HTML5 Java
implementation, you can retranslate the Java sources from the htmlparser
repository by retyping 'make translate' in this directory.

The makefile supports the following targets:

sync_htmlparser:
  Retrieves the HTML parser and Java to C++ translator sources from GitHub.
sync_javaparser:
  Retrieves the javaparser sources from GitHub.
sync:
  Runs both sync_javaparser and sync_htmlparser.
javaparser:
  Builds the javaparser library retrieved earlier by sync_javaparser.
translator:
  Runs the javaparser target and then builds the Java to C++ translator from
  sources retrieved earlier by sync_htmlparser.
libs:
  The default target. Alias for translator
translate:
  Runs the translator target and then translates the HTML parser sources
  retrieved by sync_htmlparser copying the Java sources to ../javasrc.
translate_from_snapshot:
  Runs the translator target and then translates the HTML parser sources
  stored in ../javasrc.
named_characters:
  Generates data tables for named character tokenization.

## How to add an attribute

# starting from the root of a mozilla-central checkout
cd parser/html/java/
make sync
# now you have a clone of https://github.com/validator/htmlparser/tree/master in parser/html/java/htmlparser/
cd htmlparser/src/
$EDITOR nu/validator/htmlparser/impl/AttributeName.java
# Perform case-insensitive searches for "uncomment" and uncomment stuff according to the comments that talk about uncommenting
# Duplicate the declaration a normal attribute (nothings special in SVG mode, etc.). Let's use "alt", since it's the first one.
# In the duplicate, replace ALT with the new name in all caps and "alt" with the new name in quotes in lower case.
# Search for "ALT,", duplicate that line and change the duplicate to say the new name in all caps followed by comma.
# Save.
javac nu/validator/htmlparser/impl/AttributeName.java
java nu.validator.htmlparser.impl.AttributeName
# Copy and paste the output into nu/validator/htmlparser/impl/AttributeName.java replacing the text below the comment "START GENERATED CODE" and above the very last "}".
# Recomment the bits that you uncommented earlier.
# Save.
cd ../.. # Back to parser/html/java/
make translate
cd ../../..
./mach clang-format

## How to add an element

# First, add an entry to parser/htmlparser/nsHTMLTagList.h or dom/svg/SVGTagList.h!
# Then, starting from the root of a mozilla-central checkout
cd parser/html/java/
make sync
# now you have a clone of https://github.com/validator/htmlparser/tree/master in parser/html/java/htmlparser/
cd htmlparser/src/
$EDITOR nu/validator/htmlparser/impl/ElementName.java
# Perform case-insensitive searches for "uncomment and uncomment stuff according to the comments that talk about uncommenting
# Duplicate the declaration a normal element. Let's use "bdo", since it's the first normal one.
# In the duplicate, replace BDO with the new name in all caps and "bdo" with the new name in quotes in lower case (twice).
# Search for "BDO,", duplicate that line and change the duplicate to say the new name in all caps followed by comma.
# Save.
javac nu/validator/htmlparser/impl/ElementName.java
java nu.validator.htmlparser.impl.ElementName ../../../../../parser/htmlparser/nsHTMLTagList.h ../../../../../dom/svg/SVGTagList.h
# Copy and paste the output into nu/validator/htmlparser/impl/ElementName.java replacing the text below the comment "START GENERATED CODE" and above the very last "}".
# Recomment the bits that you uncommented earlier.
# Save.
cd ../.. # Back to parser/html/java/
make translate
cd ../../..
./mach clang-format

Ben Newman (23 September 2009)
Henri Sivonen (10 August 2017, 10 February 2020)
