# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil; -*-
#
# This file is MPL/GPL dual-licensed under the following terms:
# 
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and
# limitations under the License.
#
# The Original Code is PLIF 1.0.
# The Initial Developer of the Original Code is Ian Hickson.
#
# Alternatively, the contents of this file may be used under the terms
# of the GNU General Public License Version 2 or later (the "GPL"), in
# which case the provisions of the GPL are applicable instead of those
# above. If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the MPL, indicate your decision by
# deleting the provisions above and replace them with the notice and
# other provisions required by the GPL. If you do not delete the
# provisions above, a recipient may use your version of this file
# under either the MPL or the GPL.

package PLIF::DataSource::DebugStrings;
use strict;
use vars qw(@ISA);
use PLIF::DataSource;
@ISA = qw(PLIF::DataSource);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'dataSource.strings.default' or $class->SUPER::provides($service));
}

sub getDefaultString {
    my $self = shift;
    my($app, $protocol, $string) = @_;
    # this is protocol agnostic stuff :-)
    if ($string eq 'debug.dumpVars') {
        # the slashes in regexps below have to be escaped:
        #   once for the regexp
        #   both of those for the TemplateToolkit string
        #   all four of those for Perl's <<here document string.
        # So a single literal slash in a regexp requires eight backslashes.
        return ('TemplateToolkit', '1', <<eof);
[%-

  MACRO debugDumpVarsEscape(key) BLOCK;
    IF key.search('[\\\\\\\\ \\\\\\.\\']');
      '\\'';
      key.replace('\\\\\\\\', '\\\\\\\\')
         .replace(' ',    '\\\\ ')
         .replace('\\\\\\.', '\\\\.')
         .replace('\\n',   '\\\\n')
         .replace('\\'',   '\\\\\\'');
      '\\'';
    ELSE;
      key;
    END;
  END;

  # prime the stack with the root variables
  SET debugDumpVarsStack = [];
  FOREACH debugDumpVarsKey = keys;
    NEXT IF debugDumpVarsKey == '_DEBUG' or # TemplateToolkit builtins
            debugDumpVarsKey == '_PARENT' or
            debugDumpVarsKey == 'component' or
            debugDumpVarsKey == 'dec' or
            debugDumpVarsKey == 'debugDumpVarsEscape' or
            debugDumpVarsKey == 'inc' or
            debugDumpVarsKey == 'debugDumpVarsStack' or
            debugDumpVarsKey == 'template';
    SET debugDumpVarsKeyEscaped = debugDumpVarsEscape(debugDumpVarsKey);
    debugDumpVarsStack.push([debugDumpVarsKeyEscaped, \$debugDumpVarsKey]);
  END;

  # go through the stack putting it into a hash
  SET debugDumpVarsMaxLength = 0;
  SET debugDumpVarsVariables = {};
  WHILE debugDumpVarsStack.size;
   SET frame = debugDumpVarsStack.pop;
   SET key = frame.0;
   SET variable = frame.1;
   IF (variable.ref == 'HASH');
     IF (variable.keys.size);
       FOREACH subkey = variable.keys;
         SET name = key _ '.' _ debugDumpVarsEscape(subkey);
         debugDumpVarsStack.push([name, variable.\$subkey]);
       END;
     ELSE;
       debugDumpVarsVariables.\${key} = 'empty hash';
     END;
   ELSIF (variable.ref == 'ARRAY');
     IF (variable.size);
       SET index = variable.max;
       FOREACH item = variable;
         SET name = key _ '.' _ index;
         debugDumpVarsStack.push([name, item]);
         SET index = index + 1;
       END;
     ELSE;
       debugDumpVarsVariables.\${key} = 'empty array';
     END;
   ELSE;
     IF (!variable.defined);
       debugDumpVarsVariables.\${key} = 'undefined';
     ELSIF (variable.search('^[0-9]+(?:\\\\.[0+9]+)?\$'));
       debugDumpVarsVariables.\${key} = variable;
     ELSE;
       debugDumpVarsVariables.\${key} =
         '\\'' _
         variable.replace('\\\\\\\\', '\\\\\\\\')
                 .replace('\\'',   '\\\\\\'') _
         '\\'';
     END;
   END;
   IF key.length > debugDumpVarsMaxLength;
     debugDumpVarsMaxLength = key.length;
   END;
  END;

  MACRO debugDumpVarsPad BLOCK;
    key | padright(debugDumpVarsMaxLength);
  END;

  SET debugDumpVarsMaxLengthIndent = debugDumpVarsMaxLength + 7;
  FOREACH debugDumpVarsVariables;
    '  ';
    debugDumpVarsPad(key).replace(' ', '.');
    '...: ';
    value | indentlines(0, debugDumpVarsMaxLengthIndent); "\\n";
  END;

-%]
eof
    } else {
        return;
    }
}
