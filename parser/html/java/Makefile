# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

libs:: translator

translator:: javaparser \
; mkdir -p htmlparser/bin && \
  find htmlparser/translator-src/nu/validator/htmlparser -name "*.java" | \
    xargs javac -cp javaparser.jar -g -d htmlparser/bin && \
  jar cfm translator.jar manifest.txt -C htmlparser/bin .

javaparser:: \
; mkdir -p javaparser/bin && \
  find javaparser/src -name "*.java" | \
    xargs javac -encoding ISO-8859-1 -g -d javaparser/bin && \
  jar cf javaparser.jar -C javaparser/bin .

sync_javaparser:: \
; if [ ! -d javaparser/.git ] ; \
    then rm -rf javaparser ; \
    git clone https://github.com/javaparser/javaparser.git ; \
  fi ; \
  cd javaparser ; git checkout javaparser-1.0.6 ; cd ..

sync_htmlparser:: \
; if [ -d htmlparser/.hg ] ; \
  then echo "The htmlparser repo has move to GitHub. Please remove the htmlparser directory and resync." ; exit ; \
  elif [ ! -d htmlparser/.git ] ; \
    then rm -rf htmlparser ; \
    git clone https://github.com/validator/htmlparser.git ; \
    cd htmlparser ; git checkout master ; cd .. ; \
  fi

sync:: sync_javaparser sync_htmlparser

translate:: translator \
; mkdir -p ../javasrc ; \
  java -jar translator.jar \
    htmlparser/src/nu/validator/htmlparser/impl \
    .. ../../../xpcom/ds/StaticAtoms.py ../../../xpcom/ds/HTMLAtoms.py

translate_from_snapshot:: translator \
; mkdir -p ../javasrc ; \
  java -jar translator.jar \
    ../javasrc \
    .. ../../../xpcom/ds/StaticAtoms.py ../../../xpcom/ds/HTMLAtoms.py

named_characters:: translator \
; java -cp translator.jar \
    nu.validator.htmlparser.generator.GenerateNamedCharactersCpp \
      named-character-references.html ../
