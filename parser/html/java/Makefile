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
  then cd htmlparser ; hg pull --rebase ; cd .. ; \
  else \
    rm -rf htmlparser ; \
    hg clone https://hg.mozilla.org/projects/htmlparser ; \
  fi

sync:: sync_javaparser sync_htmlparser

translate:: translator \
; mkdir -p ../javasrc ; \
  java -jar translator.jar \
    htmlparser/src/nu/validator/htmlparser/impl \
    .. ../nsHtml5AtomList.h

translate_from_snapshot:: translator \
; mkdir -p ../javasrc ; \
  java -jar translator.jar \
    ../javasrc \
    .. ../nsHtml5AtomList.h

named_characters:: translator \
; java -cp translator.jar \
    nu.validator.htmlparser.generator.GenerateNamedCharactersCpp \
      named-character-references.html ../

clean_javaparser:: \
; rm -rf javaparser/bin javaparser.jar

clean_htmlparser:: \
; rm -rf htmlparser/bin translator.jar

clean:: clean_javaparser clean_htmlparser
