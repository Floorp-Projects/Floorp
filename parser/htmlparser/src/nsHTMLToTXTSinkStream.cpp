/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *     Greg Kostello (original structure)
 *     Akkana Peck <akkana@netscape.com>
 *     Daniel Bratell <bratell@lysator.liu.se>
 *     Ben Bucksch <mozilla@bucksch.org>
 *     Pierre Phaneuf <pp@ludusdesign.com>
 */

/**
 * MODULE NOTES:
 * 
 * This file declares the concrete TXT ContentSink class.
 * This class is used during the parsing process as the
 * primary interface between the parser and the content
 * model.
 */

#include "nsHTMLToTXTSinkStream.h"
#include "nsHTMLTokens.h"
#include "nsString.h"
#include "nsIParser.h"
#include "nsHTMLEntities.h"
#include "nsXIFDTD.h"
#include "prprf.h"         // For PR_snprintf()
#include "nsIDocumentEncoder.h"    // for output flags
#include "nsIUnicodeEncoder.h"
#include "nsICharsetAlias.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsIOutputStream.h"
#include "nsFileStream.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

const  PRInt32 gTabSize=4;
const  PRInt32 gOLNumberWidth = 3;
const  PRInt32 gIndentSizeList = (gTabSize > gOLNumberWidth+3) ? gTabSize: gOLNumberWidth+3;
                               // Indention of non-first lines of ul and ol

static PRBool IsInline(eHTMLTags aTag);
static PRBool IsBlockLevel(eHTMLTags aTag);

/**
 *  Inits the encoder instance variable for the sink based on the charset 
 *  
 *  @update  gpk 4/21/99
 *  @param   aCharset
 *  @return  NS_xxx error result
 */
nsresult nsHTMLToTXTSinkStream::InitEncoder(const nsString& aCharset)
{
  nsresult res = NS_OK;
  
  // If the converter is ucs2, then do not use a converter
  if (aCharset.Equals("ucs2"))
  {
    NS_IF_RELEASE(mUnicodeEncoder);
    return res;
  }

  nsICharsetAlias* calias = nsnull;
  res = nsServiceManager::GetService(kCharsetAliasCID,
                                     kICharsetAliasIID,
                                     (nsISupports**)&calias);

  NS_ASSERTION( nsnull != calias, "cannot find charset alias");
  nsAutoString charsetName = aCharset;
  if( NS_SUCCEEDED(res) && (nsnull != calias))
  {
    res = calias->GetPreferred(aCharset, charsetName);
    nsServiceManager::ReleaseService(kCharsetAliasCID, calias);

    if(NS_FAILED(res))
    {
       // failed - unknown alias , fallback to ISO-8859-1
      charsetName = "ISO-8859-1";
    }

    nsICharsetConverterManager * ccm = nsnull;
    res = nsServiceManager::GetService(kCharsetConverterManagerCID, 
                                       NS_GET_IID(nsICharsetConverterManager), 
                                       (nsISupports**)&ccm);
    if(NS_SUCCEEDED(res) && (nsnull != ccm))
    {
      nsIUnicodeEncoder * encoder = nsnull;
      res = ccm->GetUnicodeEncoder(&charsetName, &encoder);
      if(NS_SUCCEEDED(res) && (nsnull != encoder))
      {
        NS_IF_RELEASE(mUnicodeEncoder);
        mUnicodeEncoder = encoder;
      }    
      nsServiceManager::ReleaseService(kCharsetConverterManagerCID, ccm);
    }
  }
  return res;
}

/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gpk02/03/99
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 */
nsresult
nsHTMLToTXTSinkStream::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  if(aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = (nsIContentSink*)(this);
  }
  else if(aIID.Equals(NS_GET_IID(nsIContentSink))) {
    *aInstancePtr = (nsIContentSink*)(this);
  }
  else if(aIID.Equals(NS_GET_IID(nsIHTMLContentSink))) {
    *aInstancePtr = (nsIHTMLContentSink*)(this);
  }
  else if(aIID.Equals(NS_GET_IID(nsIHTMLToTXTSinkStream))) {
    *aInstancePtr = (nsIHTMLToTXTSinkStream*)(this);
  }
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;                                                        
}

NS_IMPL_ADDREF(nsHTMLToTXTSinkStream)
NS_IMPL_RELEASE(nsHTMLToTXTSinkStream)

// Someday may want to make this non-const:
static const PRUint32 TagStackSize = 500;
static const PRUint32 OLStackSize = 100;

/**
 * Construct a content sink stream.
 * @update      gpk02/03/99
 * @param 
 * @return
 */
nsHTMLToTXTSinkStream::nsHTMLToTXTSinkStream()
{
  NS_INIT_REFCNT();
  mColPos = 0;
  mIndent = 0;
  mCiteQuoteLevel = 0;
  mDoFragment = PR_FALSE;
  mBufferSize = 0;
  mBufferLength = 0;
  mBuffer = nsnull;
  mUnicodeEncoder = nsnull;
  mWrapColumn = 72;     // XXX magic number, we expect someone to reset this

  // Flow
  mEmptyLines=1; // The start of the document is an "empty line" in itself,
  mCurrentLine = "";
  mInWhitespace = PR_TRUE;
  mPreFormatted = PR_FALSE;
  mCacheLine = PR_FALSE;

  // initialize the tag stack to zero:
  mTagStack = new nsHTMLTag[TagStackSize];
  mTagStackIndex = 0;

  // initialize the OL stack, where numbers for ordered lists are kept:
  mOLStack = new PRInt32[OLStackSize];
  mOLStackIndex = 0;
}

/**
 * 
 * @update      gpk02/03/99
 * @param 
 * @return
 */
nsHTMLToTXTSinkStream::~nsHTMLToTXTSinkStream()
{
  NS_WARN_IF_FALSE(mCurrentLine.Length() == 0, "Buffer not flushed! Probably illegal input to class.");

  if(mBuffer) 
    delete[] mBuffer;
  delete[] mTagStack;
  delete[] mOLStack;
  NS_IF_RELEASE(mUnicodeEncoder);
}

/**
 * 
 * @update      gpk04/30/99
 * @param 
 * @return
 */
NS_IMETHODIMP
nsHTMLToTXTSinkStream::Initialize(nsIOutputStream* aOutStream, 
                                  nsString* aOutString,
                                  PRUint32 aFlags)
{
  mStream = aOutStream;
  mString = aOutString;
  mFlags = aFlags;

  return NS_OK;
}

/**
 * 
 * @update      gpk04/30/99
 * @param 
 * @return
 */
NS_IMETHODIMP
nsHTMLToTXTSinkStream::SetCharsetOverride(const nsString* aCharset)
{
  if (aCharset)
  {
    mCharsetOverride = *aCharset;
    InitEncoder(mCharsetOverride);
  }
  return NS_OK;
}

/**
  * This method gets called by the parser when it encounters
  * a title tag and wants to set the document title in the sink.
  *
  * @update gpk02/03/99
  * @param  nsString reference to new title value
  * @return PR_TRUE if successful. 
  */
NS_IMETHODIMP
nsHTMLToTXTSinkStream::SetTitle(const nsString& aValue)
{
  return NS_OK;
}

/**
  * All these HTML-specific methods may be called, or may not,
  * depending on whether the parser is parsing XIF or HTML.
  * So we can't depend on them; instead, we have Open/CloseContainer
  * do all the specialized work, and the html-specific Open/Close
  * methods must call the more general methods.
  * Since there are so many of them, make a macro:
  */

#define USE_GENERAL_OPEN_METHOD(opentag) \
NS_IMETHODIMP \
nsHTMLToTXTSinkStream::opentag(const nsIParserNode& aNode) \
{ return OpenContainer(aNode); }

#define USE_GENERAL_CLOSE_METHOD(closetag) \
NS_IMETHODIMP \
nsHTMLToTXTSinkStream::closetag(const nsIParserNode& aNode) \
{ return CloseContainer(aNode); }

USE_GENERAL_OPEN_METHOD(OpenHTML)
USE_GENERAL_CLOSE_METHOD(CloseHTML)
USE_GENERAL_OPEN_METHOD(OpenHead)
USE_GENERAL_CLOSE_METHOD(CloseHead)
USE_GENERAL_OPEN_METHOD(OpenBody)
USE_GENERAL_CLOSE_METHOD(CloseBody)
USE_GENERAL_OPEN_METHOD(OpenForm)
USE_GENERAL_CLOSE_METHOD(CloseForm)
USE_GENERAL_OPEN_METHOD(OpenMap)
USE_GENERAL_CLOSE_METHOD(CloseMap)
USE_GENERAL_OPEN_METHOD(OpenFrameset)
USE_GENERAL_CLOSE_METHOD(CloseFrameset)

NS_IMETHODIMP
nsHTMLToTXTSinkStream::DoFragment(PRBool aFlag) 
{
  mDoFragment = aFlag;
  return NS_OK; 
}

/**
 * This gets called when handling illegal contents, especially
 * in dealing with tables. This method creates a new context.
 * 
 * @update 04/04/99 harishd
 * @param aPosition - The position from where the new context begins.
 */
NS_IMETHODIMP
nsHTMLToTXTSinkStream::BeginContext(PRInt32 aPosition) 
{
  return NS_OK;
}

/**
 * This method terminates any new context that got created by
 * BeginContext and switches back to the main context.  
 *
 * @update 04/04/99 harishd
 * @param aPosition - Validates the end of a context.
 */
NS_IMETHODIMP
nsHTMLToTXTSinkStream::EndContext(PRInt32 aPosition)
{
  return NS_OK;
}

/**
 *  This gets called by the parser when you want to add
 *  a PI node to the current container in the content
 *  model.
 *  
 *  @updated gpk02/03/99
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLToTXTSinkStream::AddProcessingInstruction(const nsIParserNode& aNode){
  return NS_OK;
}

/**
 *  This gets called by the parser when it encounters
 *  a DOCTYPE declaration in the HTML document.
 */
NS_IMETHODIMP
nsHTMLToTXTSinkStream::AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode)
{
  return NS_OK;
}

/**
 *  This gets called by the parser when you want to add
 *  a comment node to the current container in the content
 *  model.
 *  
 *  @updated gpk02/03/99
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsHTMLToTXTSinkStream::AddComment(const nsIParserNode& aNode)
{
  // Skip comments in plaintext output
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLToTXTSinkStream::GetValueOfAttribute(const nsIParserNode& aNode,
                                           char* aMatchKey,
                                           nsString& aValueRet)
{
  nsAutoString matchKey (aMatchKey);
  PRInt32 count=aNode.GetAttributeCount();
  for (PRInt32 i=0;i<count;i++)
  {
    const nsString& key = aNode.GetKeyAt(i);
    if (key == matchKey)
    {
      aValueRet = aNode.GetValueAt(i);
      return NS_OK;
    }
  }
  return NS_ERROR_NOT_AVAILABLE;
}
    
PRBool nsHTMLToTXTSinkStream::DoOutput()
{
  PRBool inBody = PR_FALSE;

  // Loop over the tag stack and see if we're inside a body,
  // and not inside a markup_declaration
  for (PRUint32 i = 0; i < mTagStackIndex; ++i)
  {
    if (mTagStack[i] == eHTMLTag_markupDecl
        || mTagStack[i] == eHTMLTag_comment)
      return PR_FALSE;

    if (mTagStack[i] == eHTMLTag_body)
      inBody = PR_TRUE;
  }

  return mDoFragment || inBody;
}

nsAutoString
Spaces(PRInt32 count)
{
  nsAutoString result;
  while (result.Length() < count)
    result += ' ';
  return result;
}
    
/**
  * This method is used to a general container. 
  * This includes: OL,UL,DIR,SPAN,TABLE,H[1..6],etc.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::OpenContainer(const nsIParserNode& aNode)
{
  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
#ifdef DEBUG_bratell
  printf("OpenContainer: %d    ", type);
#endif  
  const nsString&   name = aNode.GetText();
  if (name.Equals("document_info"))
  {
    nsString value;
    if (NS_SUCCEEDED(GetValueOfAttribute(aNode, "charset", value)))
    {
      if (mCharsetOverride.Length() == 0)
        InitEncoder(value);
      else
        InitEncoder(mCharsetOverride);
    }
    return NS_OK;
  }

  if (mTagStackIndex < TagStackSize)
    mTagStack[mTagStackIndex++] = type;

  if (type == eHTMLTag_body)
  {
    //    body ->  can turn on cacheing unless it's already preformatted
    if(!(mFlags & nsIDocumentEncoder::OutputPreformatted) && 
       ((mFlags & nsIDocumentEncoder::OutputFormatted) ||
        (mFlags & nsIDocumentEncoder::OutputWrap))) {
      mCacheLine = PR_TRUE;
    }
    
    // Try to figure out here whether we have a
    // preformatted style attribute.
    //
    // Trigger on the presence of a "-moz-pre-wrap" in the
    // style attribute. That's a very simplistic way to do
    // it, but better than nothing.
    // Also set mWrapColumn to the value given there
    // (which arguably we should only do if told to do so).
    nsString style;
    PRInt32 whitespace;
    if(NS_SUCCEEDED(GetValueOfAttribute(aNode, "style", style)) &&
       (-1 != (whitespace = style.Find("white-space:"))))
       /* DELETEME: What, if the style is defined in an external stylesheet? */
    {
      if (-1 != style.Find("-moz-pre-wrap", PR_TRUE, whitespace))
      {
#ifdef DEBUG_preformatted
        printf("Set mPreFormatted based on style moz-pre-wrap\n");
#endif
        mPreFormatted = PR_TRUE;
        mCacheLine = PR_TRUE;
        PRInt32 widthOffset = style.Find("width:");
        if (widthOffset >= 0)
        {
          // We have to search for the ch before the semicolon,
          // not for the semicolon itself, because nsString::ToInteger()
          // considers 'c' to be a valid numeric char (even if radix=10)
          // but then gets confused if it sees it next to the number
          // when the radix specified was 10, and returns an error code.
          PRInt32 semiOffset = style.Find("ch", widthOffset+6);
          PRInt32 length = (semiOffset > 0 ? semiOffset - widthOffset - 6
                            : style.Length() - widthOffset);
          nsString widthstr;
          style.Mid(widthstr, widthOffset+6, length);
          PRInt32 err;
          PRInt32 col = widthstr.ToInteger(&err);
          if (NS_SUCCEEDED(err))
          {
            SetWrapColumn((PRUint32)col);
#ifdef DEBUG_preformatted
            printf("Set wrap column to %d based on style\n", mWrapColumn);
#endif
          }
        }
      }
      else if (-1 != style.Find("pre", PR_TRUE, whitespace))
      {
#ifdef DEBUG_preformatted
        printf("Set mPreFormatted based on style pre\n");
#endif
        mPreFormatted = PR_TRUE;
        mCacheLine = PR_TRUE;
        SetWrapColumn(0);
      }
    } else {
      mPreFormatted = PR_FALSE;
      mCacheLine = PR_TRUE; // Cache lines unless something else tells us not to
    }

    return NS_OK;
  }

  if (!DoOutput())
    return NS_OK;

  if (type == eHTMLTag_p || type == eHTMLTag_pre)
    EnsureVerticalSpace(1); // Should this be 0 in unformatted case?

  // Else make sure we'll separate block level tags,
  // even if we're about to leave before doing any other formatting.
  // Oddly, I can't find a case where this actually makes any difference.
  //else if (IsBlockLevel(type))
  //  EnsureVerticalSpace(0);

  // The rest of this routine is formatted output stuff,
  // which we should skip if we're not formatted:
  if (!(mFlags & nsIDocumentEncoder::OutputFormatted))
    return NS_OK;

  if (type == eHTMLTag_ul)
  {
    // Indent here to support nested list, which aren't included in li :-(
    EnsureVerticalSpace(1); // Must end the current line before we change indent.
    mIndent += gIndentSizeList;
  }
  else if (type == eHTMLTag_ol)
  {
    EnsureVerticalSpace(1); // Must end the current line before we change indent.
    if (mOLStackIndex < OLStackSize)
      mOLStack[mOLStackIndex++] = 1;  // XXX should get it from the node!
    mIndent += gIndentSizeList;  // see ul
  }
  else if (type == eHTMLTag_li)
  {
    if (mTagStackIndex > 1 && mTagStack[mTagStackIndex-2] == eHTMLTag_ol)
    {
      if (mOLStackIndex > 0)
        // This is what nsBulletFrame does for OLs:
        mInIndentString.Append(mOLStack[mOLStackIndex-1]++, 10);
      else
        mInIndentString.Append("#");

      mInIndentString.Append('.');

    }
    else
      mInIndentString.Append('*');
    
    mInIndentString.Append(' ');
  }
  else if (type == eHTMLTag_blockquote)
  {
    EnsureVerticalSpace(0);
    // Find out whether it's a type=cite, and insert "> " instead.
    // Eventually we should get the value of the pref controlling citations,
    // and handle AOL-style citations as well.
    // If we want to support RFC 2646 (and we do!) we have to have:
    // >>>> text
    // >>> fdfd
    // when a mail is sent.
    nsString value;
    if (NS_SUCCEEDED(GetValueOfAttribute(aNode, "type", value))
        && value.StripChars("\"").Equals("cite", PR_TRUE))
      mCiteQuoteLevel++;
    else
      mIndent += gTabSize; // Check for some maximum value?
  }
  else if (type == eHTMLTag_a)
  {
    nsAutoString url;
    if (NS_SUCCEEDED(GetValueOfAttribute(aNode, "href", url))
        && !url.IsEmpty())
      mURL = url.StripChars("\"");
  }
  else if (type == eHTMLTag_img)
  {
    nsAutoString url;
    if (NS_SUCCEEDED(GetValueOfAttribute(aNode, "src", url)) && !url.IsEmpty())
    {
      nsAutoString temp, desc;
      if (NS_SUCCEEDED(GetValueOfAttribute(aNode, "alt", desc))
          && !desc.IsEmpty())
      {
        temp += " (";
        temp += desc.StripChars("\"");
        temp += " <";
        temp += url.StripChars("\"");
        temp += ">) ";
      }
      else
      {
        temp += " <";
        temp += url.StripChars("\"");
        temp += "> ";
      }
      Write(temp);
    }
  }
  else if (type == eHTMLTag_sup)
    Write("^");
  // I don't know a plain text representation of sub
  else if (type == eHTMLTag_strong || type == eHTMLTag_b)
    Write("*");
  else if (type == eHTMLTag_em || type == eHTMLTag_i)
    Write("/");
  else if (type == eHTMLTag_u)
    Write("_");

  return NS_OK;
}

/**
  * This method is used to close a generic container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::CloseContainer(const nsIParserNode& aNode)
{
  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
#ifdef DEBUG_bratell
  printf("CloseContainer: %d    ", type);
#endif
  if (mTagStackIndex > 0)
    --mTagStackIndex;

  // End current line if we're ending a block level tag
  if (IsBlockLevel(type)) {
    if((type == eHTMLTag_body) || (type == eHTMLTag_html)) {
      // We want the output to end with a new line,
      // but in preformatted areas like text fields,
      // we can't emit newlines that weren't there.
      // So add the newline only in the case of formatted output.
      if (mFlags & nsIDocumentEncoder::OutputFormatted)
        EnsureVerticalSpace(0);
      else
        FlushLine();
    } else if ((type == eHTMLTag_tr) ||
               (type == eHTMLTag_li) ||
               (type == eHTMLTag_pre) ||
               (type == eHTMLTag_blockquote)) {
      EnsureVerticalSpace(0);
    } else {
      // All other blocks get 1 vertical space after them
      // in formatted mode, otherwise 0.
      // This is hard. Sometimes 0 is a better number, but
      // how to know?
      EnsureVerticalSpace((mFlags & nsIDocumentEncoder::OutputFormatted) ? 1 : 0);
    }
  }

  // The rest of this routine is formatted output stuff,
  // which we should skip if we're not formatted:
  if (!(mFlags & nsIDocumentEncoder::OutputFormatted))
    return NS_OK;

  if (type == eHTMLTag_ul)
  {
    mIndent -= gIndentSizeList;
  }
  else if (type == eHTMLTag_ol)
  {
    FlushLine(); // Doing this after decreasing OLStackIndex would be wrong.
    --mOLStackIndex;
    mIndent -= gIndentSizeList;
  }
  else if (type == eHTMLTag_blockquote)
  {
    FlushLine();
    if (mCiteQuoteLevel>0)
      mCiteQuoteLevel--;
    else if(mIndent >= gTabSize)
      mIndent -= gTabSize;
  }
  else if (type == eHTMLTag_td)
  {
    // We are after a table cell an thus maybe between two cells.
    // Something should be done to avoid the two cells to be written
    // together. This really need some intelligence about how the
    // contents in the cell looks.
    // Fow now, I will only add a SPACE. Could be a TAB or something
    // else but I'm not sure everything can handle the TAB so SPACE
    // seems like a better solution.
    if(!mInWhitespace) {
      // Maybe add something else? Several spaces? A TAB? SPACE+TAB?
      if(mCacheLine) {
        AddToLine(nsAutoString(" ").GetUnicode(), 1);
      } else {
        nsAutoString space(" ");
        WriteSimple(space);
      }
      mInWhitespace = PR_TRUE;
    }
  }
  else if (type == eHTMLTag_a)
  { // these brackets must stay here
    if (!mURL.IsEmpty())
    {
      nsAutoString temp(" <");
      temp += mURL;
      temp += ">";
      Write(temp);
      mURL.Truncate();
    }
  }
  else if (type == eHTMLTag_sup)
    Write(" ");
  else if (type == eHTMLTag_strong || type == eHTMLTag_b)
    Write("*");
  else if (type == eHTMLTag_em || type == eHTMLTag_i)
    Write("/");
  else if (type == eHTMLTag_u)
    Write("_");

  return NS_OK;
}


/**
  * This method is used to add a leaf to the currently 
  * open container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::AddLeaf(const nsIParserNode& aNode)
{
#ifdef DEBUG_bratell
  printf("Addleaf: %d (%d)   ", (eHTMLTags)aNode.GetNodeType(),mFlags);
#endif
  
  // If we don't want any output, just return
  if (!DoOutput())
    return NS_OK;
  
  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
  
  nsString text = aNode.GetText();

#ifdef DEBUG_bratell
  printf("        '%s'    ", text.ToNewCString());
#endif

  if (mTagStackIndex > 1 && mTagStack[mTagStackIndex-2] == eHTMLTag_select)                                                                    
  {                                                                             
  // Don't output the contents of SELECT elements;                            
// Might be nice, eventually, to output just the selected element.          
    return NS_OK;                                                               
  }                                                                             
  else if (type == eHTMLTag_text)     
  {
    Write(text);
  } 
  else if (type == eHTMLTag_entity)
  {
    PRUnichar entity = nsHTMLEntities::EntityToUnicode(aNode.GetText());
    nsAutoString temp;
    temp.Append(entity);
    Write(temp);
  }
  else if (type == eHTMLTag_br)
  {
    // Do this even if we're not doing formatted output:
    EnsureVerticalSpace(mEmptyLines+1);
  }
  else if (type == eHTMLTag_whitespace)
  {
    // The only times we want to pass along whitespace from the original
    // html source are if we're forced into preformatted mode via flags,
    // or if we're prettyprinting and we're inside a <pre>.
    // Otherwise, either we're collapsing to minimal text, or we're
    // prettyprinting to mimic the html format, and in neither case
    // does the formatting of the html source help us.
    if (mFlags & nsIDocumentEncoder::OutputPreformatted ||
        ((mFlags & nsIDocumentEncoder::OutputFormatted)
         && (mTagStackIndex > 0)
         && (mTagStack[mTagStackIndex-1] == eHTMLTag_pre)) ||
        (mPreFormatted && !mWrapColumn))
      {
        Write(text); // XXX: spacestuffing (maybe call AddToLine if mCacheLine==true)
      } else if(!mInWhitespace) {
        Write(" ");
        mInWhitespace = PR_TRUE;
      }
  }
  else if (type == eHTMLTag_newline)
  {
    if (mFlags & nsIDocumentEncoder::OutputPreformatted ||
        ((mFlags & nsIDocumentEncoder::OutputFormatted)
         && (mTagStackIndex > 0)
         && (mTagStack[mTagStackIndex-1] == eHTMLTag_pre)) ||
        (mPreFormatted && !mWrapColumn))
    {
      EnsureVerticalSpace(mEmptyLines+1);
    }
  }
  else if (type == eHTMLTag_hr &&
           (mFlags & nsIDocumentEncoder::OutputFormatted))
  {
    EnsureVerticalSpace(0);

    // Make a line of dashes as wide as the wrap width
    nsAutoString line;
    int width = (mWrapColumn > 0 ? mWrapColumn : 25);
    while (line.Length() < width)
      line += '-';
    Write(line);

    EnsureVerticalSpace(0);
  }
  
  return NS_OK;
}

void nsHTMLToTXTSinkStream::EnsureBufferSize(PRInt32 aNewSize)
{  
  if (mBufferSize < aNewSize)
  {
    nsAllocator::Free(mBuffer);
    mBufferSize = 2*aNewSize+1; // make the twice as large
    mBuffer = NS_STATIC_CAST(char*, nsAllocator::Alloc(mBufferSize));
    if(mBuffer){
      mBuffer[0] = 0;
      mBufferLength = 0;
    }
  }
}

void nsHTMLToTXTSinkStream::EncodeToBuffer(nsString& aSrc)
{
  // First, replace all nbsp characters with spaces,
  // which the unicode encoder won't do for us.
  PRUnichar nbsp = 160;
  PRUnichar space = ' ';
  aSrc.ReplaceChar(nbsp, space);

  if (mUnicodeEncoder == nsnull)
  {
    NS_WARNING("The unicode encoder needs to be initialized");
    EnsureBufferSize(aSrc.Length()+1);
    aSrc.ToCString ( mBuffer, aSrc.Length()+1 );
    return;
  }

  PRInt32       length = aSrc.Length();
  nsresult      result;

  if (mUnicodeEncoder != nsnull && length > 0)
  {
    EnsureBufferSize(length);
    mBufferLength = mBufferSize;
    
    mUnicodeEncoder->Reset();
    result = mUnicodeEncoder->Convert(aSrc.GetUnicode(), &length, mBuffer, &mBufferLength);
    mBuffer[mBufferLength] = 0;
    PRInt32 temp = mBufferLength;
    if (NS_SUCCEEDED(result))
      result = mUnicodeEncoder->Finish(mBuffer,&temp);
  }
}

void
nsHTMLToTXTSinkStream::EnsureVerticalSpace(PRInt32 noOfRows)
{
  while(mEmptyLines < noOfRows)
    EndLine(PR_FALSE);
}

// This empties the current line cache without adding a NEWLINE.
// Should not be used if line wrapping is of importance since
// this function destroys the cache information.
void
nsHTMLToTXTSinkStream::FlushLine()
{
  if(mCurrentLine.Length()>0) {
    if(0 == mColPos)
      WriteQuotesAndIndent();


    WriteSimple(mCurrentLine);
    mColPos += mCurrentLine.Length();
    mCurrentLine.Truncate();
  }
}

/**
 *  WriteSimple places the contents of aString into either the output stream
 *  or the output string.
 *  When going to the stream, all data is run through the encoder.
 *  No formatting or wrapping is done here; that happens in ::Write.
 *  
 *  @updated gpk02/03/99
 *  @param   
 *  @return  
 */
void nsHTMLToTXTSinkStream::WriteSimple(nsString& aString)
{
  // If a encoder is being used then convert first convert the input string
  if (mUnicodeEncoder != nsnull)
  {
    EncodeToBuffer(aString);
    if (mStream != nsnull)
    {
      nsOutputStream out(mStream);
      out.write(mBuffer,mBufferLength);
    }
    if (mString != nsnull)
    {
      mString->Append(mBuffer);
    }
  }
  else
  {
    if (mStream != nsnull)
    {
      nsOutputStream out(mStream);
      const PRUnichar* unicode = aString.GetUnicode();
      PRUint32   length = aString.Length();
      out.write(unicode,length);
    }
    else
    {
      mString->Append(aString);
    }
  }
}

void
nsHTMLToTXTSinkStream::AddToLine(const PRUnichar * aLineFragment, PRInt32 aLineFragmentLength)
{
  PRUint32 prefixwidth = (mCiteQuoteLevel>0?mCiteQuoteLevel+1:0)+mIndent;
  
  PRInt32 linelength = mCurrentLine.Length();
  if(0 == linelength) {
    if(0 == aLineFragmentLength) {
      // Nothing at all. Are you kidding me?
      return;
    }

    if(mFlags & nsIDocumentEncoder::OutputFormatFlowed) {
      if(('>' == aLineFragment[0]) ||
         (' ' == aLineFragment[0]) ||
         (!nsCRT::strncmp(aLineFragment, "From ", 5))) {
        // Space stuffing a la RFC 2646 if this will be used in a mail,
        // but how can I know that??? Now space stuffing is done always
        // when formatting text as HTML and that is wrong! XXX: Fix this!
        mCurrentLine.Append(' ');
      }
    }
    mEmptyLines=-1;
  }
    
  mCurrentLine.Append(aLineFragment, aLineFragmentLength);
  
  linelength = mCurrentLine.Length();

  //  Wrap?
  if(mWrapColumn &&
     ((mFlags & nsIDocumentEncoder::OutputFormatted) ||
      (mFlags & nsIDocumentEncoder::OutputWrap)))
  {
    // Yes, wrap!
    // The "+4" is to avoid wrap lines that only should be a couple
    // of letters too long.
    while(linelength+prefixwidth > mWrapColumn+4) {
      // Must wrap. Let's find a good place to do that.
      PRInt32 goodSpace = mWrapColumn-prefixwidth;
      while (goodSpace >= 0 &&
             !nsCRT::IsAsciiSpace(mCurrentLine.CharAt(goodSpace))) {
        goodSpace--;
      }
    
      nsAutoString restOfLine = "";
      if(goodSpace<0) {
        // If we don't found a good place to break, accept long line and
        // try to find another place to break
        goodSpace=mWrapColumn-prefixwidth;
        while (goodSpace < linelength &&
               !nsCRT::IsAsciiSpace(mCurrentLine.CharAt(goodSpace))) {
          goodSpace++;
        }
      }

      if(goodSpace < linelength && goodSpace > 0) {
        // Found a place to break
        mCurrentLine.Right(restOfLine, linelength-goodSpace-1);
        mCurrentLine.Cut(goodSpace, linelength-goodSpace);
        EndLine(PR_TRUE);
        mCurrentLine.Truncate();
        // Space stuff new line?
        if(mFlags & nsIDocumentEncoder::OutputFormatFlowed) {
          if((restOfLine[0] == '>') ||
             (restOfLine[0] == ' ') ||
             (!restOfLine.Compare("From ",PR_FALSE,5))) {
            // Space stuffing a la RFC 2646 if this will be used in a mail,
            // but how can I know that??? Now space stuffing is done always
            // when formatting text as HTML and that is wrong! XXX: Fix this!
            mCurrentLine.Append(' ');
          }
        }
        mCurrentLine.Append(restOfLine);
        linelength = mCurrentLine.Length();
        mEmptyLines = -1;
      } else {
        // Nothing to do. Hopefully we get more data later
        // to use for a place to break line
        break;
      }
    }
  } else {
    // No wrapping.
  }
}

void
nsHTMLToTXTSinkStream::EndLine(PRBool softlinebreak)
{
  if(softlinebreak) {
    if(0 == mCurrentLine.Length()) {
      // No meaning
      return;
    }
    WriteQuotesAndIndent();
    // Remove SPACE from the end of the line.
    while(' ' == mCurrentLine[mCurrentLine.Length()-1])
      mCurrentLine.SetLength(mCurrentLine.Length()-1);
    if(mFlags & nsIDocumentEncoder::OutputFormatFlowed) {
      // Add the soft part of the soft linebreak (RFC 2646 4.1)
      mCurrentLine.Append(' ');
    }
    mCurrentLine.Append(NS_LINEBREAK);
    WriteSimple(mCurrentLine);
    mCurrentLine.Truncate();
    mColPos=0;
    mEmptyLines=0;
    mInWhitespace=PR_TRUE;
  } else {
    // Hard break
    if(0 == mColPos) {
      WriteQuotesAndIndent();
    }
    if(mCurrentLine.Length()>0)
      mEmptyLines=-1;
    // Output current line
    // Remove SPACE from the end of the line.
    while(' ' == mCurrentLine[mCurrentLine.Length()-1])
      mCurrentLine.SetLength(mCurrentLine.Length()-1);
    mCurrentLine.Append(NS_LINEBREAK);
    WriteSimple(mCurrentLine);
    mCurrentLine.Truncate();
    mColPos=0;
    mEmptyLines++;
    mInWhitespace=PR_TRUE;
  }
}

void
nsHTMLToTXTSinkStream::WriteQuotesAndIndent()
{
  // Put the mail quote "> " chars in, if appropriate:
  if (mCiteQuoteLevel>0) {
    nsAutoString quotes;
    for(int i=0; i<mCiteQuoteLevel; i++) {
      quotes.Append('>');
    }
    quotes.Append(' ');
    WriteSimple(quotes);
    mColPos += (mCiteQuoteLevel+1);
  }
  
  // Indent if necessary
  PRInt32 indentwidth = mIndent - mInIndentString.Length();
  if (indentwidth > 0) {
    nsAutoString spaces;
    for (int i=0; i<indentwidth; ++i)
      spaces.Append(' ');
    WriteSimple(spaces);
    mColPos += indentwidth;
  }
  
  if(mInIndentString.Length()>0) {
    WriteSimple(mInIndentString);
    mColPos += mInIndentString.Length();
    mInIndentString.Truncate();
  }
  
}

#ifdef DEBUG_akkana_not
#define DEBUG_wrapping 1
#endif

//
// Write a string, wrapping appropriately to mWrapColumn.
// This routine also handles indentation and mail-quoting,
// and so should be used for formatted output even if we're not wrapping.
//
void
nsHTMLToTXTSinkStream::Write(const nsString& aString)
{
#ifdef DEBUG_wrapping
  char* foo = aString.ToNewCString();
  printf("Write(%s): wrap col = %d, mColPos = %d\n", foo, mWrapColumn, mColPos);
  nsAllocator::Free(foo);
#endif

  PRInt32 bol = 0;
  PRInt32 newline;
  
  PRInt32 totLen = aString.Length();
  
  // Don't wrap mail-quoted text
  // Yes do! /Daniel Bratell
  //  PRUint32 wrapcol = (mCiteQuote ? 0 : mWrapColumn);
  //  PRInt32 prefixwidth = (mCiteQuoteLevel>0?mCiteQuoteLevel+1:0)+mIndent;
  //  PRInt32 linewidth = mWrapColumn-prefixwidth;
  //  if ((!(mFlags & nsIDocumentEncoder::OutputFormatted)
  //       && !(mFlags & nsIDocumentEncoder::OutputWrap)) ||
  //      ((mTagStackIndex > 0) &&
  //       (mTagStack[mTagStackIndex-1] == eHTMLTag_pre)))
  if (((mTagStackIndex > 0) &&
       (mTagStack[mTagStackIndex-1] == eHTMLTag_pre)) ||
      (mPreFormatted && !mWrapColumn))
  {
    // No intelligent wrapping. This mustn't be mixed with
    // intelligent wrapping without clearing the mCurrentLine
    // buffer before!!!

    NS_WARN_IF_FALSE(mCurrentLine.Length() == 0,
                 "Mixed wrapping data and nonwrapping data on the same line");
    if (mCurrentLine.Length() > 0)
      FlushLine();
    
    // Put the mail quote "> " chars in, if appropriate.
    // Have to put it in before every line.
    while(bol<totLen) {
      if(0 == mColPos)
        WriteQuotesAndIndent();
      
      newline = aString.FindChar('\n',PR_FALSE,bol);
      
      if(newline < 0) {
        // No new lines.
        nsAutoString stringpart;
        aString.Right(stringpart, totLen-bol);
        if(stringpart.Length()>0) {
          PRUnichar lastchar = stringpart[stringpart.Length()-1];
          if((lastchar == '\t') || (lastchar == ' ') ||
             (lastchar == '\r') ||(lastchar == '\n')) {
            mInWhitespace = PR_TRUE;
          } else {
            mInWhitespace = PR_FALSE;
          }
        }
        WriteSimple(stringpart);
        mEmptyLines=-1;
        mColPos += totLen-bol;
        bol = totLen;
      } else {
        nsAutoString stringpart;
        aString.Mid(stringpart, bol, newline-bol+1);
        mInWhitespace = PR_TRUE;
        WriteSimple(stringpart);
        mEmptyLines=0;
        mColPos=0;
        bol = newline+1;
      }
    }

#ifdef DEBUG_wrapping
    printf("No wrapping: newline is %d, totLen is %d; leaving mColPos = %d\n",
           newline, totLen, mColPos);
#endif
    return;
  }

  // Intelligent handling of text
  // If needed, strip out all "end of lines"
  // and multiple whitespace between words
  PRInt32 nextpos;
  nsAutoString tempstr;
  const PRUnichar * offsetIntoBuffer = nsnull;
  
  while (bol < totLen) {    // Loop over lines
    // Find a place where we may have to do whitespace compression
    nextpos = aString.FindCharInSet(" \t\n\r", bol);
#ifdef DEBUG_wrapping
    nsString remaining;
    aString.Right(remaining, totLen - bol);
    foo = remaining.ToNewCString();
    //    printf("Next line: bol = %d, newlinepos = %d, totLen = %d, string = '%s'\n",
    //           bol, nextpos, totLen, foo);
    nsAllocator::Free(foo);
#endif

    if(nextpos < 0) {
      // The rest of the string
      if(!mCacheLine) {
        aString.Right(tempstr, totLen-bol);
        WriteSimple(tempstr);
      } else {
        offsetIntoBuffer = aString.GetUnicode();
        offsetIntoBuffer = &offsetIntoBuffer[bol];
        AddToLine(offsetIntoBuffer, totLen-bol);
      }
      bol=totLen;
      mInWhitespace=PR_FALSE;
    } else {
      if(mInWhitespace && (nextpos == bol) &&
         !(mFlags & nsIDocumentEncoder::OutputPreformatted)) {
        // Skip whitespace
        bol++;
        continue;
      }

      if(nextpos == bol) {
        // Note that we are in whitespace.
        mInWhitespace = PR_TRUE;
        if(!mCacheLine) {
          nsAutoString whitestring=aString[nextpos];
          WriteSimple(whitestring);
        } else {
          offsetIntoBuffer = aString.GetUnicode();
          offsetIntoBuffer = &offsetIntoBuffer[nextpos];
          AddToLine(offsetIntoBuffer, 1);
        }
        bol++;
        continue;
      }

      
      if(!mCacheLine) {
        aString.Mid(tempstr,bol,nextpos-bol);
        if(mFlags & nsIDocumentEncoder::OutputPreformatted) {
          bol = nextpos;
        } else {
          tempstr.Append(" ");
          bol = nextpos + 1;
          mInWhitespace = PR_TRUE;
        }
        WriteSimple(tempstr);
      } else {
         mInWhitespace = PR_TRUE;
         
         offsetIntoBuffer = aString.GetUnicode();
         offsetIntoBuffer = &offsetIntoBuffer[bol];
         if(mFlags & nsIDocumentEncoder::OutputPreformatted) {
           // Preserve the real whitespace character
           nextpos++;
           AddToLine(offsetIntoBuffer, nextpos-bol);
           bol = nextpos;
         } else {
           // Replace the whitespace with a space
           AddToLine(offsetIntoBuffer, nextpos-bol);
           AddToLine(nsAutoString(" ").GetUnicode(),1);
           bol = nextpos + 1; // Let's eat the whitespace
         }
      }
    }
  } // Continue looping over the string
}

/**
  * This method gets called when the parser begins the process
  * of building the content model via the content sink.
  *
  * @update gpk02/03/99
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::WillBuildModel(void){
  return NS_OK;
}

/**
  * This method gets called when the parser concludes the process
  * of building the content model via the content sink.
  *
  * @param  aQualityLevel describes how well formed the doc was.
  *         0=GOOD; 1=FAIR; 2=POOR;
  * @update gpk02/03/99
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::DidBuildModel(PRInt32 aQualityLevel) {
  return NS_OK;
}

/**
  * This method gets called when the parser gets i/o blocked,
  * and wants to notify the sink that it may be a while before
  * more data is available.
  *
  * @update gpk02/03/99
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::WillInterrupt(void) {
  return NS_OK;
}

/**
  * This method gets called when the parser i/o gets unblocked,
  * and we're about to start dumping content again to the sink.
  *
  * @update gpk02/03/99
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::WillResume(void) {
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLToTXTSinkStream::SetParser(nsIParser* aParser) {
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLToTXTSinkStream::NotifyError(const nsParserError* aError)
{
  return NS_OK;
}

PRBool IsInline(eHTMLTags aTag)
{
  PRBool  result = PR_FALSE;

  switch (aTag)
  {
    case  eHTMLTag_a:
    case  eHTMLTag_address:
    case  eHTMLTag_big:
    case  eHTMLTag_blink:
    case  eHTMLTag_b:
    case  eHTMLTag_br:
    case  eHTMLTag_cite:
    case  eHTMLTag_code:
    case  eHTMLTag_dfn:
    case  eHTMLTag_em:
    case  eHTMLTag_font:
    case  eHTMLTag_img:
    case  eHTMLTag_i:
    case  eHTMLTag_kbd:
    case  eHTMLTag_keygen:
    case  eHTMLTag_nobr:
    case  eHTMLTag_samp:
    case  eHTMLTag_small:
    case  eHTMLTag_spacer:
    case  eHTMLTag_span:      
    case  eHTMLTag_strike:
    case  eHTMLTag_strong:
    case  eHTMLTag_sub:
    case  eHTMLTag_sup:
    case  eHTMLTag_td:
    case  eHTMLTag_textarea:
    case  eHTMLTag_tt:
    case  eHTMLTag_u:
    case  eHTMLTag_var:
    case  eHTMLTag_wbr:
      result = PR_TRUE;
      break;

    default:
      break;
  }
  return result;
}

PRBool IsBlockLevel(eHTMLTags aTag) 
{
  return !IsInline(aTag);
}
