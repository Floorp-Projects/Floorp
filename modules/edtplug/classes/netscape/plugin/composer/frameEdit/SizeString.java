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

package netscape.plugin.composer.frameEdit;

import netscape.plugin.composer.*;
import netscape.plugin.composer.io.*;
import java.io.*;
import java.util.StringTokenizer;
import netscape.application.*;
import netscape.util.*;

/** Encapsulates the Frameset layout policy.
 */

class SizeString {
    public SizeString(){
        this("");
    }
    public SizeString(String string) {
        StringTokenizer tokenizer = new StringTokenizer(string,SEP);
        while ( tokenizer.hasMoreTokens()){
            String token = tokenizer.nextToken();
            validate(token);
            entries.addElement(token);
        }
    }
    public String toString() {
        StringBuffer buffer = new StringBuffer();
        Enumeration e = entries.elements();
        while ( e.hasMoreElements() ){
            buffer.append((String) e.nextElement());
            if ( e.hasMoreElements() ) {
                buffer.append(SEP);
            }
        }
        return buffer.toString();
    }
    public int length() {
        return entries.size();
    }

    public int typeOf(int index){
        String size = sizeAt(index);
        int length = size.length();
        if ( length < 1 ) return TYPE_FIXED;
        char c = size.charAt(length-1);
        int result = TYPE_FIXED;
        switch ( c ) {
            case '*':
                result = TYPE_PERCENT_FREE;
                break;
            case '%':
                result = TYPE_PERCENT_NORMAL;
            default:
                break;
        }
        return result;
    }

    public int valueOf(int index){
        String size = sizeAt(index);
        // Based on inspection of ns/lib/layout/laygrid.c lo_parse_percent_list
        int length = size.length();
        int i = 0;
        while(i < length && Character.isDigit(size.charAt(i))){
            i++;
        }
        if ( i <= 0 ){
            size = "1";
            i = 1;
            length = 1;
        }
        int value = Integer.parseInt(size.substring(0,i));
        if ( value <= 0 ){
            if ( typeOf(index) == TYPE_PERCENT_NORMAL ) {
                i = 100 / length;
            }
            if ( i <= 0 ) {
                i = 1;
            }
        }
        return value;
    }

    public String sizeAt(int index){
        if ( index < 0 || index >= entries.size() ) {
            return "";
        }
        return (String) entries.elementAt(index);
    }
    public String setSizeAt(String string, int index){
        validate(string);
        String result = (String) entries.elementAt(index);
        entries.setElementAt(string, index);
        return result;
    }
    public void insertSizeAt(String string, int index){
        validate(string);
        entries.insertElementAt(string, index);
    }
    public void addSize(String string){
        insertSizeAt(string, length());
    }
    public String removeSizeAt(int index){
        String result = (String) entries.elementAt(index);
        entries.removeElementAt(index);
        return result;
    }
    protected void validate(String size){
        int length = size.length();
        if ( length > 0 && size.charAt(length - 1 ) == 0){
            System.err.println("Bad size string.");
        }
    }

    public void normalize(int frames){
        while ( length() < frames ) {
            addSize("");
        }
        while ( length() > frames ){
            removeSizeAt(length() - 1);
        }
    }

    public void normalizeFree(){
        int length = length();
        int[] sizes = new int[length];
        int total = 0;
        for(int i = 0; i < length; i++){
            if ( typeOf(i) == TYPE_PERCENT_FREE ){
                sizes[i] = valueOf(i);
                total += sizes[i];
            }
            else {
                sizes[i] = -1;
            }
        }
        if ( total > 0 ) {
            if ( total != 100 ) {
                double ratio = 100.0 / (double) total;
                int newTotal = 0;
                int lastOne = 0;
                for(int i = 0; i < length; i++){
                    if ( sizes[i] > 0 ){
                        int newSize = (int) ((double) sizes[i] * ratio);
                        sizes[i] = newSize;
                        newTotal += newSize;
                    }
                }
                if ( newTotal != 100 ) {
                    sizes[lastOne] += (100 - newTotal);
                }
                for(int i = 0; i < length; i++){
                    if ( sizes[i] > 0  ){
                        setSizeAt(Integer.toString(sizes[i]) + "*", i);
                    }
                }
            }
        }
    }
    public void normalizePercent(){
        int length = length();
        int[] sizes = new int[length];
        int total = 0;
        boolean allPercent = true;
        for(int i = 0; i < length; i++){
            if ( typeOf(i) == TYPE_PERCENT_NORMAL ){
                sizes[i] = valueOf(i);
                total += sizes[i];
            }
            else {
                sizes[i] = -1;
                allPercent = false;
            }
        }
        if ( allPercent && length > 0) {
            if ( total != 100 ) {
                double ratio = 100.0 / (double) total;
                int newTotal = 0;
                int lastOne = 0;
                for(int i = 0; i < length; i++){
                    if ( sizes[i] > 0 ){
                        int newSize = (int) ((double) sizes[i] * ratio);
                        sizes[i] = newSize;
                        newTotal += newSize;
                    }
                }
                if ( newTotal != 100 ) {
                    sizes[lastOne] += (100 - newTotal);
                }
                for(int i = 0; i < length; i++){
                    if ( sizes[i] > 0  ){
                        setSizeAt(Integer.toString(sizes[i]) + "%", i);
                    }
                }
            }
        }
    }

    public int[] calcSizes(int total){
        // See lib/layout/lo_adjust_percents
        int length = length();
        if ( length <= 0 ) {
            int sizes[] = new int[1];
            sizes[0] = total;
            return sizes;
        }
        int[] sizes = new int[length];
        int[] types = new int[length];
        if ( total <= 0 ) {
            return sizes; // Pathalogical case.
        }
        // Collect the information
        int starTotal = 0;
        int used = 0;
        for(int i = 0; i < length; i++){
            types[i] = typeOf(i);
            sizes[i] = valueOf(i);
            switch ( types[i] ){
                default:
                case 0:
                    used += sizes[i];
                    break;
                case 1:
                    int percent = Math.max(0,Math.min(100, sizes[i]));
                    sizes[i] = total * percent / 100;
                    used += sizes[i];
                    break;
                case 2:
                    starTotal += sizes[i];
                    break;
            }
        }
        if ( used < total ) {
            // Total number of '*'s?
            if ( starTotal > 0 ){
                for(int i = 0; i < length; i++){
                    if ( types[i] == SizeString.TYPE_PERCENT_FREE ){
                        int part = Math.max(0, Math.min(starTotal, sizes[i]));
                        int size = (total - used) * part / starTotal;
                        sizes[i] = size;
                        used += size;
                        starTotal -= part;
                    }
                }
                if ( used < total ) {
                    sizes[length-1] += (total - used);
                    used = total;
                }
            }
        }
        if ( used > total ) {
            double ratio = ((double) used) / ((double) total);
            used = 0;
            // Cut away proportionally. This isn't exactly what Nav 3.0 does.
            for(int i = 0; i < length; i++){
                int size = (int) (sizes[i] * ratio);
                sizes[i] = size;
                used += size;
                }
            if ( used < total ) {
                sizes[length-1] += (total - used);
                used = total;
            }
        }
        /* System.err.println(sizeString);
        System.err.println(total);
        for(int i = 0; i < length; i++){
            System.err.println("item["+i+"] "+sizes[i]);
        }
        */
        return sizes;
    }
    public void splitAt(double firstPart, int index){
        int type = typeOf(index);
        if ( type == TYPE_PERCENT_FREE ){
            // Silly Season. It's difficult to split "Free". Do it
            // to the same accuracy as percent.
            normalizeFree();
        }
        String postfix = postfix(type);
        int oldValue = valueOf(index);
        int newFirstValue = (int) (oldValue * firstPart);
        int newSecondValue = oldValue - newFirstValue;
        setSizeAt(Integer.toString(newFirstValue) + postfix, index);
        insertSizeAt(Integer.toString(newSecondValue) + postfix, index + 1);
    }
    public void moveSizeAt(int totalLength, double ratio, int index){
        int type = typeOf(index);
        if ( type == TYPE_PERCENT_FREE){
            // Silly Season. It's difficult to split "Free". Do it
            // to the same accuracy as percent.
            normalizeFree();
        }
        String postfix = postfix(type);
        int sizes[] = calcSizes(totalLength);
        int pixSubTotal = sizes[index] + sizes[index + 1];
        int type1 = typeOf(index + 1);
        int value = valueOf(index);
        int value1 = valueOf(index + 1);
        int newFirstValue;
        int newSecondValue;
        if (type == TYPE_FIXED ) {
            newFirstValue = (int) (ratio * pixSubTotal);
            newSecondValue = pixSubTotal - newFirstValue;
        }
        else if (type == TYPE_PERCENT_NORMAL){
            int subTotalPercent;
            if ( type1 == TYPE_PERCENT_NORMAL ){
                subTotalPercent = value + value1;
            }
            else {
                subTotalPercent = 100 * pixSubTotal / totalLength;
            }
            newFirstValue = (int) (ratio * subTotalPercent);
            newSecondValue = subTotalPercent - newFirstValue;
        }
        else {
            int subTotalStars = valueOf(index);
            if ( type1 == TYPE_FIXED ) {
                subTotalStars += 100 * value1 / totalLength;
            }
            else {
                subTotalStars += value1;
            }
            newFirstValue = (int) ratio * subTotalStars;
            newSecondValue = subTotalStars - newFirstValue;
        }
        setSizeAt(Integer.toString(newFirstValue) + postfix, index);
        setSizeAt(Integer.toString(newSecondValue) + postfix, index + 1);
    }
    public static String postfix(int type ) {
        return postfix_[type];
    }

    private static String[] postfix_ = {"","%","*"};

    public final static int TYPE_FIXED = 0;
    public final static int TYPE_PERCENT_NORMAL = 1;
    public final static int TYPE_PERCENT_FREE = 2;
    private Vector entries = new Vector();
    private final static String SEP = ",";
}
