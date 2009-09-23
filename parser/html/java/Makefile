# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Makefile.
#
# The Initial Developer of the Original Code is
#   Mozilla Corporation.
#
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Newman <b{enjam,newma}n@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

libs:: translator

translator:: javaparser \
; mkdir -p htmlparser/bin && \
  find htmlparser/translator-src/nu/validator/htmlparser -name "*.java" | \
    xargs javac -cp javaparser.jar -g -d htmlparser/bin && \
  jar cfm translator.jar manifest.txt -C htmlparser/bin .

javaparser:: \
; mkdir -p javaparser/bin && \
  find javaparser/src -name "*.java" | \
    xargs javac -g -d javaparser/bin && \
  jar cf javaparser.jar -C javaparser/bin .

sync_javaparser:: \
; if [ -d javaparser/.svn ] ; \
  then cd javaparser ; svn up ; cd .. ; \
  else \
    rm -rf javaparser ; \
    svn co http://javaparser.googlecode.com/svn/trunk/JavaParser javaparser ; \
  fi

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

named_characters:: translator \
; java -cp translator.jar \
    nu.validator.htmlparser.generator.GenerateNamedCharactersCpp \
      named-character-references.html ../

clean_javaparser:: \
; rm -rf javaparser/bin javaparser.jar

clean_htmlparser:: \
; rm -rf htmlparser/bin translator.jar

clean:: clean_javaparser clean_htmlparser
