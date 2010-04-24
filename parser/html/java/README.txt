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
  Retrieves the HTML parser and Java to C++ translator sources from Mozilla's 
  htmlparser repository.
sync_javaparser:
  Retrieves the javaparser sources from Google Code.
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
clean_javaparser:
  Removes the build products of the javaparser target.
clean_htmlparser:
  Removes the build products of the translator target.
clean:
  Runs both clean_javaparser and clean_htmlparser.

Ben Newman (23 September 2009)
Henri Sivonen (21 April 2010)
