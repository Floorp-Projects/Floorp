/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Rick Gessner <rickg@netscape.com> (original author)
 *   Scott Collins <scc@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nsStrPrivate_h
#define __nsStrPrivate_h

#include "nscore.h"
#include "nsStrShared.h"

struct nsStr;

// there are only static methods here!
class nsStrPrivate {
 public:
 /**
  * This method initializes an nsStr for use
  *
  * @update	gess 01/04/99
  * @param  aString is the nsStr to be initialized
  * @param  aCharSize tells us the requested char size (1 or 2 bytes)
  */
  static void Initialize(nsStr& aDest,eCharSize aCharSize);

 /**
  * This method initializes an nsStr for use
  *
  * @update	gess 01/04/99
  * @param  aString is the nsStr to be initialized
  * @param  aCharSize tells us the requested char size (1 or 2 bytes)
  */
  static void Initialize(nsStr& aDest,char* aCString,PRUint32 aCapacity,PRUint32 aLength,eCharSize aCharSize,PRBool aOwnsBuffer);
 /**
  * This method destroys the given nsStr, and *MAY* 
  * deallocate it's memory depending on the setting
  * of the internal mOwnsBUffer flag.
  *
  * @update	gess 01/04/99
  * @param  aString is the nsStr to be manipulated
  */
  static void Destroy(nsStr& aDest); 

 /**
  * These methods are where memory allocation/reallocation occur. 
  *
  * @update	gess 01/04/99
  * @param  aString is the nsStr to be manipulated
  * @return  
  */
  static PRBool EnsureCapacity(nsStr& aString,PRUint32 aNewLength);
  static PRBool GrowCapacity(nsStr& aString,PRUint32 aNewLength);

 /**
  * These methods are used to append content to the given nsStr
  *
  * @update	gess 01/04/99
  * @param  aDest is the nsStr to be appended to
  * @param  aSource is the buffer to be copied from
  * @param  anOffset tells us where in source to start copying
  * @param  aCount tells us the (max) # of chars to copy
  */
  static void StrAppend(nsStr& aDest,const nsStr& aSource,PRUint32 anOffset,PRInt32 aCount);

 /**
  * These methods are used to assign contents of a source string to dest string
  *
  * @update	gess 01/04/99
  * @param  aDest is the nsStr to be appended to
  * @param  aSource is the buffer to be copied from
  * @param  anOffset tells us where in source to start copying
  * @param  aCount tells us the (max) # of chars to copy
  */
  static void StrAssign(nsStr& aDest,const nsStr& aSource,PRUint32 anOffset,PRInt32 aCount);

 /**
  * These methods are used to insert content from source string to the dest nsStr
  *
  * @update	gess 01/04/99
  * @param  aDest is the nsStr to be appended to
  * @param  aDestOffset tells us where in dest to start insertion
  * @param  aSource is the buffer to be copied from
  * @param  aSrcOffset tells us where in source to start copying
  * @param  aCount tells us the (max) # of chars to insert
  */
  static void StrInsert1into1( nsStr& aDest,PRUint32 aDestOffset,const nsStr& aSource,PRUint32 aSrcOffset,PRInt32 aCount);
  static void StrInsert1into2( nsStr& aDest,PRUint32 aDestOffset,const nsStr& aSource,PRUint32 aSrcOffset,PRInt32 aCount);
  static void StrInsert2into2( nsStr& aDest,PRUint32 aDestOffset,const nsStr& aSource,PRUint32 aSrcOffset,PRInt32 aCount);


  /**
   * Helper routines for StrInsert1into1, etc
   */
  static PRInt32 GetSegmentLength(const nsStr& aSource,
                                  PRUint32 aSrcOffset, PRInt32 aCount);
  static void AppendForInsert(nsStr& aDest, PRUint32 aDestOffset, const nsStr& aSource, PRUint32 aSrcOffset, PRInt32 theLength);

 /**
  * This method deletes chars from the given str. 
  * The given allocator may choose to resize the str as well.
  *
  * @update	gess 01/04/99
  * @param  aDest is the nsStr to be deleted from
  * @param  aDestOffset tells us where in dest to start deleting
  * @param  aCount tells us the (max) # of chars to delete
  */
  static void Delete1(nsStr& aDest,PRUint32 aDestOffset,PRUint32 aCount);
  static void Delete2(nsStr& aDest,PRUint32 aDestOffset,PRUint32 aCount);

  /**
   * helper routines for Delete1, Delete2
   */
  static PRInt32 GetDeleteLength(const nsStr& aDest, PRUint32 aDestOffset, PRUint32 aCount);

 /**
  * This method is used to truncate the given string.
  * The given allocator may choose to resize the str as well (but it's not likely).
  *
  * @update	gess 01/04/99
  * @param  aDest is the nsStr to be appended to
  * @param  aDestOffset tells us where in dest to start insertion
  * @param  aSource is the buffer to be copied from
  * @param  aSrcOffset tells us where in source to start copying
  */
  static void StrTruncate(nsStr& aDest,PRUint32 aDestOffset);

  /**
   * This method trims chars (given in aSet) from the edges of given buffer 
   *
   * @update	gess 01/04/99
   * @param   aDest is the buffer to be manipulated
   * @param   aSet tells us which chars to remove from given buffer
   * @param   aEliminateLeading tells us whether to strip chars from the start of the buffer
   * @param   aEliminateTrailing tells us whether to strip chars from the start of the buffer
   */
  static void Trim(nsStr& aDest,const char* aSet,PRBool aEliminateLeading,PRBool aEliminateTrailing);
  
  /**
   * This method compresses duplicate runs of a given char from the given buffer 
   *
   * @update	gess 01/04/99
   * @param   aDest is the buffer to be manipulated
   * @param   aSet tells us which chars to compress from given buffer
   * @param   aChar is the replacement char
   * @param   aEliminateLeading tells us whether to strip chars from the start of the buffer
   * @param   aEliminateTrailing tells us whether to strip chars from the start of the buffer
   */
  static void CompressSet1(nsStr& aDest,const char* aSet,PRBool aEliminateLeading,PRBool aEliminateTrailing);
  static void CompressSet2(nsStr& aDest,const char* aSet,PRBool aEliminateLeading,PRBool aEliminateTrailing);

  /**
   * This method removes all occurances of chars in given set from aDest 
   *
   * @update	gess 01/04/99
   * @param   aDest is the buffer to be manipulated
   * @param   aSet tells us which chars to compress from given buffer
   * @param   aChar is the replacement char
   * @param   aEliminateLeading tells us whether to strip chars from the start of the buffer
   * @param   aEliminateTrailing tells us whether to strip chars from the start of the buffer
   */
  static void StripChars1(nsStr& aDest,const char* aSet);
  static void StripChars2(nsStr& aDest,const char* aSet);

  /**
   * This method compares the data bewteen two nsStr's 
   *
   * @update	gess 01/04/99
   * @param   aStr1 is the first buffer to be compared
   * @param   aStr2 is the 2nd buffer to be compared
   * @param   aCount is the number of chars to compare
   * @param   aIgnorecase tells us whether to use a case-sensitive comparison
   * @return  -1,0,1 depending on <,==,>
   */
  static PRInt32 StrCompare1To1(const nsStr& aDest,const nsStr& aSource,
                                PRInt32 aCount,PRBool aIgnoreCase);
  static PRInt32 StrCompare2To1(const nsStr& aDest, const nsStr& aSource,
                                PRInt32 aCount, PRBool aIgnoreCase);
  static PRInt32 StrCompare2To2(const nsStr& aDest, const nsStr& aSource,
                                PRInt32 aCount);

 /**
  * These methods scan the given string for 1 or more chars in a given direction
  *
  * @update	gess 01/04/99
  * @param  aDest is the nsStr to be searched to
  * @param  aSource (or aChar) is the substr we're looking to find
  * @param  aIgnoreCase tells us whether to search in a case-sensitive manner
  * @param  anOffset tells us where in the dest string to start searching
  * @return the index of the source (substr) in dest, or -1 (kNotFound) if not found.
  */
  static PRInt32 FindSubstr1in1(const nsStr& aDest,const nsStr& aSource, PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount);
  static PRInt32 FindSubstr1in2(const nsStr& aDest,const nsStr& aSource, PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount);
  static PRInt32 FindSubstr2in2(const nsStr& aDest,const nsStr& aSource, PRInt32 anOffset,PRInt32 aCount);
  
  static PRInt32 FindChar1(const nsStr& aDest,PRUnichar aChar, PRInt32 anOffset,PRInt32 aCount);
  static PRInt32 FindChar2(const nsStr& aDest,PRUnichar aChar, PRInt32 anOffset,PRInt32 aCount);
  
  static PRInt32 RFindSubstr1in1(const nsStr& aDest,const nsStr& aSource, PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount);
  static PRInt32 RFindSubstr1in2(const nsStr& aDest,const nsStr& aSource, PRBool aIgnoreCase,PRInt32 anOffset,PRInt32 aCount);
  static PRInt32 RFindSubstr2in2(const nsStr& aDest,const nsStr& aSource, PRInt32 anOffset,PRInt32 aCount);
  
  static PRInt32 RFindChar1(const nsStr& aDest,PRUnichar aChar, PRInt32 anOffset,PRInt32 aCount);
  static PRInt32 RFindChar2(const nsStr& aDest,PRUnichar aChar, PRInt32 anOffset,PRInt32 aCount);

  static void    Overwrite(nsStr& aDest,const nsStr& aSource,PRInt32 anOffset);
  
  static char GetFindInSetFilter(const char *set)
  {
    // Calculate filter
    char filter = ~char(0); // All bits set
    while (*set) {
      filter &= ~(*set);
      ++set;
    }

    return filter;
  }
  static PRUnichar GetFindInSetFilter(const PRUnichar *set)
  {
    // Calculate filter
    PRUnichar filter = ~PRUnichar(0); // All bits set
    while (*set) {
      filter &= ~(*set);
      ++set;
    }
    return filter;
  }

#ifdef NS_STR_STATS
  static PRBool   DidAcquireMemory(void);
#endif

  /**
   * Returns a hash code for the string for use in a PLHashTable.
   */
  static PRUint32 HashCode(const nsStr& aDest);

  // A copy of PR_cnvtf with a bug fixed.
  static void cnvtf(char *buf, int bufsz, int prcsn, double fval);

#ifdef NS_STR_STATS
  /**
   * Prints an nsStr. If truncate is true, the string is only printed up to 
   * the first newline. (Note: The current implementation doesn't handle
   * non-ascii unicode characters.)
   */
  static void Print(const nsStr& aDest, FILE* out, PRBool truncate = PR_FALSE);
#endif
  
  static PRBool Alloc(nsStr& aString,PRUint32 aCount);
  static PRBool Realloc(nsStr& aString,PRUint32 aCount);
  static PRBool Free(nsStr& aString);

};

#endif
