/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

/**
 * MODULE NOTES:
 * 
 * If you've been paying attention to our many content sink classes, you may be
 * asking yourself, "why do we need yet another one?" The answer is that this 
 * implementation, unlike all the others, really sends its output a given stream
 * rather than to an actual content sink (as defined in our HTML document system).
 *
 * We use this class for a number of purposes: 
 *	 1) For actual document i/o using XIF (xml interchange format)
 *   2) For document conversions
 *   3) For debug purposes (to cause output to go to cout or a file)
 *
 * If no stream is declared in the constructor then all output goes to cout. 
 * The file is pretty printed according to the pretty printing interface. subclasses 
 * may choose to override this behavior or set runtime flags for desired results.
 */

#ifndef  NS_HTMLTOTEXTSINK_STREAM
#define  NS_HTMLTOTEXTSINK_STREAM

#include "nsIHTMLContentSink.h"

#include "nsHTMLTags.h"
#include "nsParserCIID.h"
#include "nsCOMPtr.h"

class nsIDTD;

#define NS_IHTMLTOTEXTSINKSTREAM_IID  \
  {0xa39c6bff, 0x15f0, 0x11d2, \
  {0x80, 0x41, 0x0, 0x10, 0x4b, 0x98, 0x3f, 0xd4}}


class nsIUnicodeEncoder;
class nsILineBreaker;
class nsIOutputStream;

class nsIHTMLToTXTSinkStream : public nsIHTMLContentSink {
  public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IHTMLTOTEXTSINKSTREAM_IID)
  NS_DEFINE_STATIC_CID_ACCESSOR(NS_HTMLTOTXTSINKSTREAM_CID)

  NS_IMETHOD Initialize(nsIOutputStream* aOutStream, 
                        nsAWritableString* aOutString,
                        PRUint32 aFlags) = 0;
  NS_IMETHOD SetCharsetOverride(const nsAReadableString* aCharset) = 0;
  NS_IMETHOD SetWrapColumn(PRUint32 aWrapCol) = 0;
};

class nsHTMLToTXTSinkStream : public nsIHTMLToTXTSinkStream
{
  public:

  /**
   * Standard constructor
   * @update	gpk02/03/99
   */
  nsHTMLToTXTSinkStream();

  /**
   * virtual destructor
   * @update	gpk02/03/99
   */
  virtual ~nsHTMLToTXTSinkStream();

  NS_IMETHOD Initialize(nsIOutputStream* aOutStream, 
                        nsAWritableString* aOutString,
                        PRUint32 aFlags);

  NS_IMETHOD SetCharsetOverride(const nsAReadableString* aCharset);

  // nsISupports
  NS_DECL_ISUPPORTS
 
  /*******************************************************************
   * The following methods are inherited from nsIContentSink.
   * Please see that file for details.
   *******************************************************************/
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);
  NS_IMETHOD SetParser(nsIParser* aParser);
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD NotifyError(const nsParserError* aError);
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode=0);
  NS_IMETHOD FlushPendingNotifications() { return NS_OK; }

  /*******************************************************************
   * The following methods are inherited from nsIHTMLContentSink.
   * Please see that file for details.
   *******************************************************************/
  NS_IMETHOD SetTitle(const nsString& aValue);
  NS_IMETHOD OpenHTML(const nsIParserNode& aNode);
  NS_IMETHOD CloseHTML(const nsIParserNode& aNode);
  NS_IMETHOD OpenHead(const nsIParserNode& aNode);
  NS_IMETHOD CloseHead(const nsIParserNode& aNode);
  NS_IMETHOD OpenBody(const nsIParserNode& aNode);
  NS_IMETHOD CloseBody(const nsIParserNode& aNode);
  NS_IMETHOD OpenForm(const nsIParserNode& aNode);
  NS_IMETHOD CloseForm(const nsIParserNode& aNode);
  NS_IMETHOD OpenMap(const nsIParserNode& aNode);
  NS_IMETHOD CloseMap(const nsIParserNode& aNode);
  NS_IMETHOD OpenFrameset(const nsIParserNode& aNode);
  NS_IMETHOD CloseFrameset(const nsIParserNode& aNode);
  NS_IMETHOD OpenNoscript(const nsIParserNode& aNode);
  NS_IMETHOD CloseNoscript(const nsIParserNode& aNode);
  NS_IMETHOD DoFragment(PRBool aFlag);
  NS_IMETHOD BeginContext(PRInt32 aPosition);
  NS_IMETHOD EndContext(PRInt32 aPosition);

  /*******************************************************************
   * The following methods are specific to this class.
   *******************************************************************/
  NS_IMETHOD SetWrapColumn(PRUint32 aWrapCol)   { mWrapColumn = aWrapCol; return NS_OK; };

protected:
  void EnsureBufferSize(PRInt32 aNewSize);

  nsresult InitEncoder(const nsString& aCharset);

  void AddToLine(const PRUnichar * aStringToAdd, PRInt32 aLength);
  void EndLine(PRBool softlinebreak);
  void EnsureVerticalSpace(PRInt32 noOfRows);
  void FlushLine();
  void WriteQuotesAndIndent();
  void WriteSimple(nsString& aString);
  void Write(const nsString& aString);
  void EncodeToBuffer(nsString& aString);
  NS_IMETHOD GetValueOfAttribute(const nsIParserNode& aNode,
                                 char* aMatchKey,
                                 nsString& aValueRet);
  PRBool IsConverted(const nsIParserNode& aNode);
  PRBool DoOutput();
  PRBool MayWrap();
  
  PRBool IsBlockLevel(eHTMLTags aTag);

protected:
  nsIOutputStream* mStream; 
  // XXX This is wrong. It violates XPCOM string ownership rules.
  // We're only getting away with this because instances of this
  // class are restricted to single function scope.
  nsAWritableString* mString;
  nsString         mCurrentLine;

  nsIDTD*          mDTD;

  PRInt32          mIndent;
  // mInIndentString keeps a header that has to be written in the indent.
  // That could be, for instance, the bullet in a bulleted list.
  nsString         mInIndentString;
  PRInt32          mCiteQuoteLevel;
  PRInt32          mColPos;
  PRInt32          mFlags;

  // The wrap column is how many standard sized chars (western languages)
  // should be allowed on a line. There could be less chars if the chars
  // are wider than latin chars of more if the chars are more narrow.
  PRUint32         mWrapColumn;

  // The width of the line as it will appear on the screen (approx.) 
  PRUint32         mCurrentLineWidth; 

  PRBool           mDoFragment;
  PRInt32          mEmptyLines; // Will be the number of empty lines before
                                // the current. 0 if we are starting a new
                                // line and -1 if we are in a line.
  PRBool           mInWhitespace;
  PRBool           mPreFormatted;
  PRBool           mCacheLine;   // If the line should be cached before output. This makes it possible to do smarter wrapping.
  PRBool           mStartedOutput; // we've produced at least a character

  nsString         mURL;
  PRBool           mStructs;           // Output structs (pref)
  PRInt32          mHeaderStrategy;    /* Header strategy (pref)
                                          0 = no indention
                                          1 = indention, increased with
                                              header level (default)
                                          2 = numbering and slight indention */
  PRInt32          mHeaderCounter[7];  /* For header-numbering:
                                          Number of previous headers of
                                          the same depth and in the same
                                          section.
                                          mHeaderCounter[1] for <h1> etc. */

  // The tag stack: the stack of tags we're operating on, so we can nest:
  nsHTMLTag       *mTagStack;
  PRUint32         mTagStackIndex;

  // The stack for ordered lists:
  PRInt32         *mOLStack;
  PRUint32         mOLStackIndex;

  char*            mBuffer;
  PRInt32          mBufferLength;  // The length of the data in the buffer
  PRInt32          mBufferSize;    // The actual size of the buffer, regardless of the data

  nsIUnicodeEncoder*  mUnicodeEncoder;
  nsString            mCharsetOverride;
  nsString            mLineBreak;
  nsILineBreaker*     mLineBreaker;
};

inline nsresult
NS_New_HTMLToTXT_SinkStream(nsIHTMLContentSink** aInstancePtrResult, 
                            nsIOutputStream* aOutStream,
                            const nsAReadableString* aCharsetOverride=nsnull,
                            PRUint32 aWrapColumn=0, PRUint32 aFlags=0)
{
  nsCOMPtr<nsIHTMLToTXTSinkStream> it;
  nsresult rv;

  rv = nsComponentManager::CreateInstance(nsIHTMLToTXTSinkStream::GetCID(),
                                          nsnull,
                                          NS_GET_IID(nsIHTMLToTXTSinkStream),
                                          getter_AddRefs(it));
  if (NS_SUCCEEDED(rv)) {
    rv = it->Initialize(aOutStream, nsnull, aFlags);

    if (NS_SUCCEEDED(rv)) {
      it->SetWrapColumn(aWrapColumn);
      if (aCharsetOverride != nsnull) {
        it->SetCharsetOverride(aCharsetOverride);
      }
      rv = it->QueryInterface(NS_GET_IID(nsIHTMLContentSink),
                              (void**)aInstancePtrResult);
    }
  }
  
  return rv;
}

inline nsresult
NS_New_HTMLToTXT_SinkStream(nsIHTMLContentSink** aInstancePtrResult, 
                            nsAWritableString* aOutString,
                            PRUint32 aWrapColumn=0, PRUint32 aFlags=0)
{
  nsCOMPtr<nsIHTMLToTXTSinkStream> it;
  nsresult rv;

  rv = nsComponentManager::CreateInstance(nsIHTMLToTXTSinkStream::GetCID(),
                                          nsnull,
                                          NS_GET_IID(nsIHTMLToTXTSinkStream),
                                          getter_AddRefs(it));
  if (NS_SUCCEEDED(rv)) {
    rv = it->Initialize(nsnull, aOutString, aFlags);

    if (NS_SUCCEEDED(rv)) {
      it->SetWrapColumn(aWrapColumn);
      nsAutoString ucs2; ucs2.AssignWithConversion("ucs2");
      it->SetCharsetOverride(&ucs2);
      rv = it->QueryInterface(NS_GET_IID(nsIHTMLContentSink),
                              (void**)aInstancePtrResult);
    }
  }
  
  return rv;
}


#endif

