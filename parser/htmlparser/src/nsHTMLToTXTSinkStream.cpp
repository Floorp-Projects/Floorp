/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
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

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIContentSinkIID, NS_ICONTENT_SINK_IID);
static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

const  int               gTabSize=2;

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
  nsString ucs2("ucs2");
  if (aCharset.Equals(ucs2))
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
                                       nsCOMTypeInfo<nsICharsetConverterManager>::GetIID(), 
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
  if(aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (nsIContentSink*)(this);
  }
  else if(aIID.Equals(kIContentSinkIID)) {
    *aInstancePtr = (nsIContentSink*)(this);
  }
  else if(aIID.Equals(kIHTMLContentSinkIID)) {
    *aInstancePtr = (nsIHTMLContentSink*)(this);
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


/**
 *  This method creates a new sink, it sets the stream used
 *  for the sink to aStream
 *  
 *  @update  gpk 04/30/99
 */
NS_HTMLPARS nsresult
NS_New_HTMLToTXT_SinkStream(nsIHTMLContentSink** aInstancePtrResult, 
                            nsIOutputStream* aStream,
                            const nsString* aCharsetOverride,
                            PRUint32 aWrapColumn, PRUint32 aFlags)
{
  NS_ASSERTION(aStream != nsnull, "a valid stream is required");
  nsHTMLToTXTSinkStream* it = new nsHTMLToTXTSinkStream(aStream, nsnull,
                                                        aFlags);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  it->SetWrapColumn(aWrapColumn);
  if (aCharsetOverride != nsnull)
    it->SetCharsetOverride(aCharsetOverride);
  return it->QueryInterface(kIHTMLContentSinkIID, (void **)aInstancePtrResult);
}


/**
 *  This method creates a new sink, it sets the stream used
 *  for the sink to aStream
 *  
 *  @update  gpk 04/30/99
 */
NS_HTMLPARS nsresult
NS_New_HTMLToTXT_SinkStream(nsIHTMLContentSink** aInstancePtrResult, 
                            nsString* aString,
                            PRUint32 aWrapColumn, PRUint32 aFlags)
{
  NS_ASSERTION(aString != nsnull, "a valid stream is required");
  nsHTMLToTXTSinkStream* it = new nsHTMLToTXTSinkStream(nsnull, aString,
                                                        aFlags);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  it->SetWrapColumn(aWrapColumn);
  nsString ucs2("ucs2");
  it->SetCharsetOverride(&ucs2);
  return it->QueryInterface(kIHTMLContentSinkIID, (void **)aInstancePtrResult);
}

// Someday may want to make this non-const:
static const PRUint32 TagStackSize = 500;
static const PRUint32 OLStackSize = 100;

/**
 * Construct a content sink stream.
 * @update	gpk02/03/99
 * @param 
 * @return
 */
nsHTMLToTXTSinkStream::nsHTMLToTXTSinkStream(nsIOutputStream* aStream,
                                             nsString* aString,
                                             PRUint32 aFlags)
{
  NS_INIT_REFCNT();
  mStream = aStream;
  mColPos = 0;
  mIndent = 0;
  mDoOutput = PR_FALSE;
  mBufferSize = 0;
  mBufferLength = 0;
  mBuffer = nsnull;
  mUnicodeEncoder = nsnull;
  mStream = aStream;
  mString = aString;
  mFlags = aFlags;
  mWrapColumn = 72;     // XXX magic number, we expect someone to reset this

  // initialize the tag stack to zero:
  mTagStack = new nsHTMLTag[TagStackSize];
  mTagStackIndex = 0;

  // initialize the OL stack, where numbers for ordered lists are kept:
  mOLStack = new PRInt32[OLStackSize];
  mOLStackIndex = 0;
}


/**
 * 
 * @update	gpk02/03/99
 * @param 
 * @return
 */
nsHTMLToTXTSinkStream::~nsHTMLToTXTSinkStream()
{
  delete [] mBuffer;
  delete[] mTagStack;
  delete[] mOLStack;
  NS_IF_RELEASE(mUnicodeEncoder);
}

/**
 * 
 * @update	gpk04/30/99
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
nsHTMLToTXTSinkStream::SetTitle(const nsString& aValue){
  return NS_OK;
}


/**
  * This method is used to open the outer HTML container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::OpenHTML(const nsIParserNode& aNode){
  return NS_OK;
}


/**
  * This method is used to close the outer HTML container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::CloseHTML(const nsIParserNode& aNode){
  return NS_OK;
}


/**
  * This method is used to open the only HEAD container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::OpenHead(const nsIParserNode& aNode){
  return NS_OK;
}


/**
  * This method is used to close the only HEAD container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::CloseHead(const nsIParserNode& aNode){
  return NS_OK;
}


/**
  * This method is used to open the main BODY container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::OpenBody(const nsIParserNode& aNode){
  return NS_OK;
}


/**
  * This method is used to close the main BODY container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::CloseBody(const nsIParserNode& aNode){
  return NS_OK;
}


/**
  * This method is used to open a new FORM container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::OpenForm(const nsIParserNode& aNode){
  return NS_OK;
}


/**
  * This method is used to close the outer FORM container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::CloseForm(const nsIParserNode& aNode){
  return NS_OK;
}

/**
  * This method is used to open a new FORM container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::OpenMap(const nsIParserNode& aNode){
  return NS_OK;
}


/**
  * This method is used to close the outer FORM container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::CloseMap(const nsIParserNode& aNode){
  return NS_OK;
}

    
/**
  * This method is used to open the FRAMESET container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::OpenFrameset(const nsIParserNode& aNode){
  return NS_OK;
}


/**
  * This method is used to close the FRAMESET container.
  *
  * @update 07/12/98 gpk
  * @param  nsIParserNode reference to parser node interface
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLToTXTSinkStream::CloseFrameset(const nsIParserNode& aNode){
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLToTXTSinkStream::DoFragment(PRBool aFlag) 
{
  if (aFlag)
    mDoOutput = PR_TRUE;
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

void nsHTMLToTXTSinkStream::EnsureBufferSize(PRInt32 aNewSize)
{
  if (mBufferSize < aNewSize)
  {
    delete [] mBuffer;
    mBufferSize = 2*aNewSize+1; // make the twice as large
    mBuffer = new char[mBufferSize];
    if(mBuffer){
      mBuffer[0] = 0;
      mBufferLength = 0;
    }
  }
}



void nsHTMLToTXTSinkStream::EncodeToBuffer(const nsString& aSrc)
{
  if (mUnicodeEncoder == nsnull)
  {
    NS_WARNING("The unicode encoder needs to be initialized");
    char* str = aSrc.ToNewCString();
    EnsureBufferSize(aSrc.Length()+1);
    strcpy(mBuffer, str);
    delete[] str;
    return;
  }

#define CH_NBSP 160

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


    for (PRInt32 i = 0; i < mBufferLength; i++)
    {
      if (mBuffer[i] == char(CH_NBSP))
        mBuffer[i] = ' ';
    }
  }
  
}



/**
 *  Write places the contents of aString into either the output stream
 *  or the output string.
 *  When going to the stream, all data is run through the encoder
 *  
 *  @updated gpk02/03/99
 *  @param   
 *  @return  
 */
void nsHTMLToTXTSinkStream::Write(const nsString& aString)
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
  mDoOutput = PR_FALSE;
  return NS_OK;
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
  const nsString&   name = aNode.GetText();
  if (name.Equals("XIF_DOC_INFO"))
  {
    PRInt32 count=aNode.GetAttributeCount();
    for(PRInt32 i=0;i<count;i++)
    {
      const nsString& key=aNode.GetKeyAt(i);
      const nsString& value=aNode.GetValueAt(i);

      if (key.Equals("charset"))
      {
        if (mCharsetOverride.Length() == 0)
          InitEncoder(value);
        else
          InitEncoder(mCharsetOverride);
      }
    }
  }

  if (type == eHTMLTag_body)
  {
    mDoOutput = PR_TRUE;
    return NS_OK;
  }
  if (!mDoOutput)
    return NS_OK;

  if (mTagStackIndex < TagStackSize)
    mTagStack[mTagStackIndex++] = type;

  if (type == eHTMLTag_ol)
  {
    if (mOLStackIndex < OLStackSize)
      mOLStack[mOLStackIndex++] = 1;  // XXX should get it from the node!
  }

  if (type == eHTMLTag_li)
  {
    nsString temp("*");
    if (mTagStackIndex > 1 && mTagStack[mTagStackIndex-2] == eHTMLTag_ol)
    {
      if (mOLStackIndex > 0)
      {
        // This is what nsBulletFrame does for OLs:
        char cbuf[40];
        PR_snprintf(cbuf, sizeof(cbuf), "%ld.", (mOLStack[mOLStackIndex-1])++);
        temp = cbuf;
      }
      else
        temp = "#";
    }
    Write(temp);
    mColPos++;
  }
  else if (type == eHTMLTag_blockquote)
    mIndent += gTabSize;
  else if (type == eHTMLTag_pre)
  {
    nsString temp(NS_LINEBREAK);
    Write(temp);
    mColPos = 0;
  }

  // Finally, the list of tags before which we want some vertical space:
  switch (type)
  {
    case eHTMLTag_table:
    case eHTMLTag_ul:
    case eHTMLTag_ol:
    case eHTMLTag_p:
    {
      nsString temp(NS_LINEBREAK);
      Write(temp);
      mColPos = 0;
      break;
    }
    default:
      break;
  }

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

  if (type == eHTMLTag_body)
  {
    mDoOutput = PR_FALSE;
    return NS_OK;
  }

  else if (type == eHTMLTag_comment)
  {
    mDoOutput = PR_TRUE;
    return NS_OK;
  }
  if (!mDoOutput)
    return NS_OK;

  if (mTagStackIndex > 0)
    --mTagStackIndex;

  else if (type == eHTMLTag_ol)
    --mOLStackIndex;

  else if (type == eHTMLTag_blockquote)
    mIndent -= gTabSize;

  // End current line if we're ending a block level tag
  if (IsBlockLevel(type))
  {
    if (mColPos != 0)
    {
      if (mFlags & nsIDocumentEncoder::OutputFormatted)
      {
        nsString temp(NS_LINEBREAK);
        Write(temp);
        mColPos = 0;
      }
    }
  }
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
   eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
  
  nsString text = aNode.GetText();

  if (mDoOutput == PR_FALSE)
    return NS_OK;

  if (type == eHTMLTag_text)
  {
    if (mColPos > mIndent)
    {
      nsString temp(" ");
      Write(temp);
      mColPos++;
    }

    if (mFlags & nsIDocumentEncoder::OutputFormatted)
      WriteWrapped(text);
    else
    {
      Write(text);
      mColPos += text.Length();
    }
  } 
  else if (type == eHTMLTag_entity)
  {
    PRUnichar entity = nsHTMLEntities::EntityToUnicode(aNode.GetText());

    nsAutoString temp;
    
    temp.Append(entity);
    Write(temp);

    mColPos++;
  }
  else if (type == eHTMLTag_br)
  {
    if (mFlags & nsIDocumentEncoder::OutputFormatted)
    {
      nsString temp (NS_LINEBREAK);
      Write(temp);
      mColPos = 0;
    }
  }
  // The only time we want to pass along whitespace from the original
  // html source is if we're prettyprinting and we're inside a <pre>.
  // Otherwise, either we're collapsing to minimal text, or we're
  // prettyprinting to mimic the html format, and in neither case
  // does the formatting of the html source help us.
  else if ((mFlags & nsIDocumentEncoder::OutputFormatted)
           && (mTagStackIndex > 0)
           && (mTagStack[mTagStackIndex-1] == eHTMLTag_pre))
  {
    if (type == eHTMLTag_whitespace)
    {
      text = aNode.GetText();
      Write(text);
      mColPos += text.Length();
    }
    else if (type == eHTMLTag_newline)
    {
      nsString temp(NS_LINEBREAK);
      Write(temp);
      mColPos = 0;
    }
  }

  return NS_OK;
}

//
// Write a string, wrapping appropriately to mWrapColumn.
//
void
nsHTMLToTXTSinkStream::WriteWrapped(const nsString& aString)
{
  int totLen = aString.Length();
  int charsLeft = totLen;
  while (charsLeft > 0)     // Loop over lines
  {
    // Indent at the beginning of the line, if necessary
    if (mColPos == 0 && mIndent > 0)
    {
      char* spaces = new char[mIndent+1];
      for (int i=0; i<mIndent; ++i)
        spaces[i] = ' ';
      spaces[mIndent] = '\0';
      nsString temp(spaces);
      Write (temp);
      mColPos += mIndent;
      delete[] spaces;
    }

    // Write whatever chunk of the string we can fit:
    int bol = totLen - charsLeft;
    int eol = bol + mWrapColumn - mColPos;
    if (eol > totLen)
      eol = totLen;
    else
    {
      // We need to wrap, so search backward to find last IsSpace char:
      int lastSpace = eol;
      while (lastSpace > bol && !nsString::IsSpace(aString[lastSpace]))
        --lastSpace;
      if (lastSpace == bol)
      {
        // If we reached the bol, it might just be because we were close
        // to the end already and should have wrapped last time.
        // In that case, write a linebreak and come around again.
        if (mColPos > mIndent)
        {
          nsAutoString linebreak(NS_LINEBREAK);
          Write(linebreak);
          mColPos = 0;
          continue;
        }
        // Else apparently we really can't break this line at whitespace --
        // so scan forward to the next space (if any) and dump a long line:
        while (eol > totLen && !nsString::IsSpace(aString[lastSpace]))
          ++eol;
      }
      else if (lastSpace > bol && lastSpace < eol)
        eol = lastSpace+1;
#ifdef DEBUG_akkana
      else
        printf("Wrapping: bol = %d, eol = %d, lastSpace = %d, totLen = %d\n",
               bol, eol, lastSpace, totLen);
#endif
    }

    nsAutoString lineStr;
    aString.Mid(lineStr, bol, eol-bol);

    if (eol != totLen)      // we're wrapping
    {
      lineStr.Append(NS_LINEBREAK);
      mColPos = 0;
    }
    else
      mColPos += lineStr.Length();
    Write(lineStr);
    charsLeft = totLen - eol;
  }
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

