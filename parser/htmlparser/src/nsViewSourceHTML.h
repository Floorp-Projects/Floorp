/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/**
 * MODULE NOTES:
 * @update  gess 4/8/98
 *
 *
 */

#ifndef __NS_VIEWSOURCE_HTML_
#define __NS_VIEWSOURCE_HTML_

#include "nsIDTD.h"
#include "nsISupports.h"
#include "nsHTMLTokens.h"
#include "nsIHTMLContentSink.h"
#include "nsDTDUtils.h"
#include "nsParserNode.h"

class nsIParserNode;
class nsParser;
class nsITokenizer;
class nsCParserNode;

class CIndirectTextToken : public CTextToken {
public:
  CIndirectTextToken() : CTextToken() {
    mIndirectString=0;
  }

  void SetIndirectString(const nsSubstring& aString) {
    mIndirectString=&aString;
  }

  virtual const nsSubstring& GetStringValue(void){
    return (const nsSubstring&)*mIndirectString;
  }

  const nsSubstring* mIndirectString;
};


class CViewSourceHTML: public nsIDTD
{
public:

    NS_DECL_ISUPPORTS
    NS_DECL_NSIDTD

    CViewSourceHTML();
    virtual ~CViewSourceHTML();

    /**
     * Set this to TRUE if you want the DTD to verify its
     * context stack.
     * @update  gess 7/23/98
     * @param
     * @return
     */
    virtual void SetVerification(bool aEnable);

private:
    nsresult HandleToken(CToken* aToken);

    nsresult WriteTag(PRInt32 tagType,
                      const nsSubstring &aText,
                      PRInt32 attrCount,
                      bool aTagInError);

    nsresult WriteAttributes(const nsAString& tagName, 
        nsTokenAllocator* allocator, PRInt32 attrCount, bool aOwnerInError);
    void StartNewPreBlock(void);
    // Utility method for adding attributes to the nodes we generate
    void AddAttrToNode(nsCParserStartNode& aNode,
                       nsTokenAllocator* aAllocator,
                       const nsAString& aAttrName,
                       const nsAString& aAttrValue);

    bool IsUrlAttribute(const nsAString& tagName,
                          const nsAString& attrName, const nsAString& attrValue);
    void WriteHrefAttribute(nsTokenAllocator* allocator, const nsAString& href);
    nsresult CreateViewSourceURL(const nsAString& linkUrl, nsString& viewSourceUrl);
    void WriteTextInSpan(const nsAString& text, nsTokenAllocator* allocator,
                         const nsAString& attrName, const nsAString& attrValue);
    void WriteTextInAnchor(const nsAString& text, nsTokenAllocator* allocator,
                           const nsAString& attrName, const nsAString &attrValue);
    void WriteTextInElement(const nsAString& tagName, eHTMLTags tagType,
                            const nsAString& text, nsTokenAllocator* allocator,
                            const nsAString& attrName, const nsAString& attrValue);
    const nsDependentSubstring TrimTokenValue(const nsAString& tokenValue);
    void TrimTokenValue(nsAString::const_iterator& start, 
                        nsAString::const_iterator& end);
    bool IsTokenValueTrimmableCharacter(PRUnichar ch);
    nsresult GetBaseURI(nsIURI **result);
    nsresult SetBaseURI(const nsAString& baseSpec);
    static void ExpandEntities(const nsAString& textIn, nsString& textOut);
    static void CopyPossibleEntity(nsAString::const_iterator& iter,
                                   const nsAString::const_iterator& end,
                                   nsAString& textBuffer);
    static PRInt32 ToUnicode(const nsString &strNum, PRInt32 radix, PRInt32 fallback);

protected:

    nsCString           mCharset;
    nsIHTMLContentSink* mSink;
    PRInt32             mLineNumber;
    nsITokenizer*       mTokenizer; // weak

    PRInt32             mTabSize;
    bool                mSyntaxHighlight;
    bool                mWrapLongLines;
    bool                mHasOpenRoot;
    bool                mHasOpenBody;

    nsDTDMode           mDTDMode;
    eParserCommands     mParserCommand;   //tells us to viewcontent/viewsource/viewerrors...
    eParserDocType      mDocType;
    nsCString           mMimeType;

    nsString            mFilename;
    nsCOMPtr<nsIURI>    mBaseURI; // lazy -- always use GetBaseURI().

    PRUint32            mTokenCount;

    nsCParserStartNode  mStartNode;
    nsCParserStartNode  mTokenNode;
    CIndirectTextToken  mITextToken;
    nsCParserStartNode  mErrorNode;
};

#endif
