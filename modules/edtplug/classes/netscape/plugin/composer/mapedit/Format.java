/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package netscape.plugin.composer.mapedit;

import java.util.*;

public class Format {
   /** Helper for Format.format. */
   static String formatLookup(ResourceBundle res,String key,String[] args) {
    return format(res.getString(key),args);
   }

   static String formatLookup(ResourceBundle res,String key,String arg) {
      String[] args = {arg};
      return formatLookup(res,key,args);
   }

   /** Poor man's internationalizable formatting.
     * @param formatString The format string. Occurences of %0 are replaced by
     * args[0], %1 by args[1], and so on. Use %% to signify a singe %.
     * @param args The argument array.
     * @return the formatted string.
     */
    static String format(String formatString, String[] args){
        int len = formatString.length();
        StringBuffer b = new StringBuffer(len);
        for(int index = 0; index < len; index++ ){
            char c = formatString.charAt(index);
            if ( c == '%') {
                if ( index + 1 < len ) {
                    c = formatString.charAt(++index);
                    if ( c >= '0' && c <= '9') {
                        int argIndex = c - '0';
                        if ( argIndex < args.length) {
                            b.append(args[argIndex]);
                            continue;
                        }
                        else {
                            b.append('%');
                            // And fall thru to append the character.
                        }
                    }
                }
            }
            b.append(c);
        }
        return b.toString();
    }
}
