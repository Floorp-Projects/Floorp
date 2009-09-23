If this is your first time building the HTML5 parser, you need to execute the
following commands (from this directory) to bootstrap the translation:

  make sync      # fetch remote source files and licenses
  make translate # perform the Java-to-C++ translation

If you make changes to the translator or the javaparser, you can rebuild by
retyping 'make' in this directory.  If you make changes to the HTML5 java
implementation, you can retranslate the java sources by retyping 'make
translate' in this directory.

Ben Newman (23 September 2009)
