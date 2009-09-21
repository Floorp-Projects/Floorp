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
 
#ifndef nsHtml5TreeOperation_h__
#define nsHtml5TreeOperation_h__

#include "nsIContent.h"
#include "nsHtml5DocumentMode.h"
#include "nsHtml5HtmlAttributes.h"

class nsHtml5TreeOpExecutor;

enum eHtml5TreeOperation {
  // main HTML5 ops
  eTreeOpAppend,
  eTreeOpDetach,
  eTreeOpAppendChildrenToNewParent,
  eTreeOpFosterParent,
  eTreeOpAppendToDocument,
  eTreeOpAddAttributes,
  eTreeOpDocumentMode,
  eTreeOpCreateElement,
  eTreeOpSetFormElement,
  eTreeOpCreateTextNode,
  eTreeOpCreateComment,
  eTreeOpCreateDoctype,
  // Gecko-specific on-pop ops
  eTreeOpRunScript,
  eTreeOpDoneAddingChildren,
  eTreeOpDoneCreatingElement,
  eTreeOpUpdateStyleSheet,
  eTreeOpProcessBase,
  eTreeOpProcessMeta,
  eTreeOpProcessOfflineManifest,
  eTreeOpMarkMalformedIfScript,
  eTreeOpStartLayout
};

class nsHtml5TreeOperationStringPair {
  private:
    nsString mPublicId;
    nsString mSystemId;
  public:
    nsHtml5TreeOperationStringPair(const nsAString& aPublicId, 
                                   const nsAString& aSystemId)
      : mPublicId(aPublicId)
      , mSystemId(aSystemId) {
      MOZ_COUNT_CTOR(nsHtml5TreeOperationStringPair);
    }
    
    ~nsHtml5TreeOperationStringPair() {
      MOZ_COUNT_DTOR(nsHtml5TreeOperationStringPair);    
    }
    
    inline void Get(nsAString& aPublicId, nsAString& aSystemId) {
      aPublicId.Assign(mPublicId);
      aSystemId.Assign(mSystemId);
    }
};

class nsHtml5TreeOperation {

  public:
    nsHtml5TreeOperation();

    ~nsHtml5TreeOperation();

    inline void Init(nsIContent** aNode, nsIContent** aParent) {
      mOne.node = aNode;
      mTwo.node = aParent;
    }

    inline void Init(eHtml5TreeOperation aOpCode, nsIContent** aNode) {
      mOpCode = aOpCode;
      mOne.node = aNode;
    }

    inline void Init(eHtml5TreeOperation aOpCode, 
                     nsIContent** aNode,
                     nsIContent** aParent) {
      mOpCode = aOpCode;
      mOne.node = aNode;
      mTwo.node = aParent;
    }

    inline void Init(eHtml5TreeOperation aOpCode,
                     nsIContent** aNode,
                     nsIContent** aParent, 
                     nsIContent** aTable) {
      mOpCode = aOpCode;
      mOne.node = aNode;
      mTwo.node = aParent;
      mThree.node = aTable;
    }

    inline void Init(nsHtml5DocumentMode aMode) {
      mOpCode = eTreeOpDocumentMode;
      mOne.mode = aMode;
    }
    
    inline void Init(PRInt32 aNamespace, 
                     nsIAtom* aName, 
                     nsHtml5HtmlAttributes* aAttributes,
                     nsIContent** aTarget) {
      mOpCode = eTreeOpCreateElement;
      mInt = aNamespace;
      mOne.node = aTarget;
      mTwo.atom = aName;
      if (aAttributes == nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES) {
        mThree.attributes = nsnull;
      } else {
        mThree.attributes = aAttributes;
      }
    }

    inline void Init(eHtml5TreeOperation aOpCode, 
                     PRUnichar* aBuffer, 
                     PRInt32 aLength, 
                     nsIContent** aTarget) {
      mOpCode = aOpCode;
      mOne.node = aTarget;
      mTwo.unicharPtr = aBuffer;
      mInt = aLength;
    }
    
    inline void Init(nsIContent** aElement,
                     nsHtml5HtmlAttributes* aAttributes) {
      mOpCode = eTreeOpAddAttributes;
      mOne.node = aElement;
      mTwo.attributes = aAttributes;
    }
    
    inline void Init(nsIAtom* aName, 
                     const nsAString& aPublicId, 
                     const nsAString& aSystemId, nsIContent** aTarget) {
      mOpCode = eTreeOpCreateDoctype;
      mOne.atom = aName;
      mTwo.stringPair = new nsHtml5TreeOperationStringPair(aPublicId, aSystemId);
      mThree.node = aTarget;
    }

    inline PRBool IsRunScript() {
      return mOpCode == eTreeOpRunScript;
    }

    nsresult Perform(nsHtml5TreeOpExecutor* aBuilder);

    inline already_AddRefed<nsIAtom> Reget(nsIAtom* aAtom) {
      if (!aAtom || aAtom->IsStaticAtom()) {
        return aAtom;
      }
      nsAutoString str;
      aAtom->ToString(str);
      return do_GetAtom(str);
    }

  private:
  
    // possible optimization:
    // Make the queue take items the size of pointer and make the op code
    // decide how many operands it dequeues after it.
    eHtml5TreeOperation mOpCode;
    union {
      nsIContent**                    node;
      nsIAtom*                        atom;
      nsHtml5HtmlAttributes*          attributes;
      nsHtml5DocumentMode             mode;
      PRUnichar*                      unicharPtr;
      nsHtml5TreeOperationStringPair* stringPair;
    }                   mOne, mTwo, mThree;
    PRInt32             mInt; // optimize this away later by using an end 
                              // pointer instead of string length and distinct
                              // element creation opcodes for HTML, MathML and
                              // SVG.
};

#endif // nsHtml5TreeOperation_h__
