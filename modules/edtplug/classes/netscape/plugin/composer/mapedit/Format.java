/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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
