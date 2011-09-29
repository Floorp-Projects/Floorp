/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is HTML Parser C++ Translator code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
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

#ifndef nsHtml5AtomTable_h_
#define nsHtml5AtomTable_h_

#include "nsHashKeys.h"
#include "nsTHashtable.h"
#include "nsAutoPtr.h"
#include "nsIAtom.h"
#include "nsIThread.h"

class nsHtml5Atom;

class nsHtml5AtomEntry : public nsStringHashKey
{
  public:
    nsHtml5AtomEntry(KeyTypePointer aStr);
    nsHtml5AtomEntry(const nsHtml5AtomEntry& aOther);
    ~nsHtml5AtomEntry();
    inline nsHtml5Atom* GetAtom() {
      return mAtom;
    }
  private:
    nsAutoPtr<nsHtml5Atom> mAtom;
};

/**
 * nsHtml5AtomTable provides non-locking lookup and creation of atoms for 
 * nsHtml5Parser or nsHtml5StreamParser.
 *
 * The hashtable holds dynamically allocated atoms that are private to an 
 * instance of nsHtml5Parser or nsHtml5StreamParser. (Static atoms are used on 
 * interned nsHtml5ElementNames and interned nsHtml5AttributeNames. Also, when 
 * the doctype name is 'html', that identifier needs to be represented as a 
 * static atom.)
 *
 * Each instance of nsHtml5Parser has a single instance of nsHtml5AtomTable, 
 * and each instance of nsHtml5StreamParser has a single instance of 
 * nsHtml5AtomTable. Dynamic atoms obtained from an nsHtml5AtomTable are valid 
 * for == comparison with each other or with atoms declared in nsHtml5Atoms 
 * within the nsHtml5Tokenizer and the nsHtml5TreeBuilder instances owned by 
 * the same nsHtml5Parser/nsHtml5StreamParser instance that owns the 
 * nsHtml5AtomTable instance.
 * 
 * Dynamic atoms (atoms whose IsStaticAtom() returns PR_FALSE) obtained from 
 * nsHtml5AtomTable must be re-obtained from another atom table when there's a 
 * need to migrate atoms from an nsHtml5Parser to its nsHtml5StreamParser 
 * (re-obtain from the other nsHtml5AtomTable), from an nsHtml5Parser to its 
 * owner nsHtml5Parser (re-obtain from the other nsHtml5AtomTable) or from the 
 * parser to the DOM (re-obtain from the application-wide atom table). To 
 * re-obtain an atom from another atom table, obtain a string from the atom 
 * using ToString(nsAString&) and look up an atom in the other table using that 
 * string.
 *
 * An instance of nsHtml5AtomTable that belongs to an nsHtml5Parser is only 
 * accessed from the main thread. An instance of nsHtml5AtomTable that belongs 
 * to an nsHtml5StreamParser is accessed both from the main thread and from the 
 * thread that executes the runnables of the nsHtml5StreamParser instance. 
 * However, the threads never access the nsHtml5AtomTable instance concurrently 
 * in the nsHtml5StreamParser case.
 *
 * Methods on the atoms obtained from nsHtml5AtomTable may be called on any 
 * thread, although they only need to be called on the main thread or on the 
 * thread working for the nsHtml5StreamParser when nsHtml5AtomTable belongs to 
 * an nsHtml5StreamParser.
 *
 * Dynamic atoms obtained from nsHtml5AtomTable are deleted when the 
 * nsHtml5AtomTable itself is destructed, which happens when the owner 
 * nsHtml5Parser or nsHtml5StreamParser is destructed.
 */
class nsHtml5AtomTable
{
  public:
    nsHtml5AtomTable();
    ~nsHtml5AtomTable();
    
    /**
     * Must be called after the constructor before use. Returns PR_TRUE
     * when successful and PR_FALSE on OOM failure.
     */
    inline bool Init() {
      return mTable.Init();
    }
    
    /**
     * Obtains the atom for the given string in the scope of this atom table.
     */
    nsIAtom* GetAtom(const nsAString& aKey);
    
    /**
     * Empties the table.
     */
    void Clear() {
      mTable.Clear();
    }
    
#ifdef DEBUG
    void SetPermittedLookupThread(nsIThread* aThread) {
      mPermittedLookupThread = aThread;
    }
#endif  
  
  private:
    nsTHashtable<nsHtml5AtomEntry> mTable;
#ifdef DEBUG
    nsCOMPtr<nsIThread>            mPermittedLookupThread;
#endif
};

#endif // nsHtml5AtomTable_h_
