/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=78: */
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
 * The Original Code is Mozilla Communicator client code.
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
#include "nsIAtom.h"
#include "nsLoggingSink.h"
#include "nsHTMLTags.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "prprf.h"


static NS_DEFINE_IID(kIContentSinkIID, NS_ICONTENT_SINK_IID);
static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);
static NS_DEFINE_IID(kILoggingSinkIID, NS_ILOGGING_SINK_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsresult
NS_NewHTMLLoggingSink(nsIContentSink** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsLoggingSink* it = new nsLoggingSink();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIContentSinkIID, (void**) aInstancePtrResult);
}

nsLoggingSink::nsLoggingSink() {
  mOutput = 0;
  mLevel=-1;
  mSink=0;
  mParser=0;
}

nsLoggingSink::~nsLoggingSink() { 
  mSink=0;
  if(mOutput && mAutoDeleteOutput) {
    delete mOutput;
  }
  mOutput=0;
}

NS_IMPL_ISUPPORTS3(nsLoggingSink, nsILoggingSink, nsIContentSink, nsIHTMLContentSink)

NS_IMETHODIMP
nsLoggingSink::SetOutputStream(PRFileDesc *aStream,PRBool autoDeleteOutput) {
  mOutput = aStream;
  mAutoDeleteOutput=autoDeleteOutput;
  return NS_OK;
}

static
void WriteTabs(PRFileDesc * out,int aTabCount) {
  int tabs;
  for(tabs=0;tabs<aTabCount;++tabs)
    PR_fprintf(out, "  ");
}

NS_IMETHODIMP
nsLoggingSink::WillTokenize() {
  return NS_OK;
}

NS_IMETHODIMP
nsLoggingSink::WillBuildModel() {
  
  WriteTabs(mOutput,++mLevel);
  PR_fprintf(mOutput, "<begin>\n");
  
  //proxy the call to the real sink if you have one.
  if(mSink) {
    mSink->WillBuildModel();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLoggingSink::DidBuildModel() {
  
  WriteTabs(mOutput,--mLevel);
  PR_fprintf(mOutput, "</begin>\n");

  //proxy the call to the real sink if you have one.
  nsresult theResult=NS_OK;
  if(mSink) {
    theResult=mSink->DidBuildModel();
  }

  return theResult;
}

NS_IMETHODIMP
nsLoggingSink::WillInterrupt() {
  nsresult theResult=NS_OK;

  //proxy the call to the real sink if you have one.
  if(mSink) {
    theResult=mSink->WillInterrupt();
  }
  
  return theResult;
}

NS_IMETHODIMP
nsLoggingSink::WillResume() {
  nsresult theResult=NS_OK;

  //proxy the call to the real sink if you have one.
  if(mSink) {
    theResult=mSink->WillResume();
  }
  
  return theResult;
}

NS_IMETHODIMP
nsLoggingSink::SetParser(nsIParser* aParser)  {
  nsresult theResult=NS_OK;

  //proxy the call to the real sink if you have one.
  if(mSink) {
    theResult=mSink->SetParser(aParser);
  }
  
  NS_IF_RELEASE(mParser);
  
  mParser = aParser;
  
  NS_IF_ADDREF(mParser);

  return theResult;
}

NS_IMETHODIMP
nsLoggingSink::OpenContainer(const nsIParserNode& aNode) {

  OpenNode("container", aNode); //do the real logging work...

  nsresult theResult=NS_OK;

  //then proxy the call to the real sink if you have one.
  if(mSink) {
    theResult=mSink->OpenContainer(aNode);
  }
  
  return theResult;

}

NS_IMETHODIMP
nsLoggingSink::CloseContainer(const nsHTMLTag aTag) {

  nsresult theResult=NS_OK;

  nsHTMLTag nodeType = nsHTMLTag(aTag);
  if ((nodeType >= eHTMLTag_unknown) &&
      (nodeType <= nsHTMLTag(NS_HTML_TAG_MAX))) {
    const PRUnichar* tag = nsHTMLTags::GetStringValue(nodeType);
    theResult = CloseNode(NS_ConvertUTF16toUTF8(tag).get());
  }
  else theResult= CloseNode("???");

  //then proxy the call to the real sink if you have one.
  if(mSink) {
    theResult=mSink->CloseContainer(aTag);
  }
  
  return theResult;

}

NS_IMETHODIMP
nsLoggingSink::AddLeaf(const nsIParserNode& aNode) {
  LeafNode(aNode);

  nsresult theResult=NS_OK;

  //then proxy the call to the real sink if you have one.
  if(mSink) {
    theResult=mSink->AddLeaf(aNode);
  }
  
  return theResult;

} 

/**
 *  This gets called by the parser when you want to add
 *  a PI node to the current container in the content
 *  model.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsLoggingSink::AddProcessingInstruction(const nsIParserNode& aNode){

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos)*2);
#endif

  nsresult theResult=NS_OK;

  //then proxy the call to the real sink if you have one.
  if(mSink) {
    theResult=mSink->AddProcessingInstruction(aNode);
  }
  
  return theResult;
}

/**
 *  This gets called by the parser when it encounters
 *  a DOCTYPE declaration in the HTML document.
 */

NS_IMETHODIMP
nsLoggingSink::AddDocTypeDecl(const nsIParserNode& aNode) {

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos)*2);
#endif

  nsresult theResult=NS_OK;

  //then proxy the call to the real sink if you have one.
  if(mSink) {
    theResult=mSink->AddDocTypeDecl(aNode);
  }
  
  return theResult;

}

/**
 *  This gets called by the parser when you want to add
 *  a comment node to the current container in the content
 *  model.
 *  
 *  @updated gess 3/25/98
 *  @param   
 *  @return  
 */
NS_IMETHODIMP
nsLoggingSink::AddComment(const nsIParserNode& aNode){

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos)*2);
#endif

  nsresult theResult=NS_OK;

  //then proxy the call to the real sink if you have one.
  if(mSink) {
    theResult=mSink->AddComment(aNode);
  }
  
  return theResult;

}

NS_IMETHODIMP
nsLoggingSink::OpenHead() {
  WriteTabs(mOutput,++mLevel);
  PR_fprintf(mOutput,"<open container=head>\n");

  nsresult theResult=NS_OK;

  //then proxy the call to the real sink if you have one.
  if(mSink) {
    theResult=mSink->OpenHead();
  }
  
  return theResult;
}

nsresult
nsLoggingSink::OpenNode(const char* aKind, const nsIParserNode& aNode) {
	WriteTabs(mOutput,++mLevel);

  PR_fprintf(mOutput,"<open container=");

  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
  if ((nodeType >= eHTMLTag_unknown) &&
      (nodeType <= nsHTMLTag(NS_HTML_TAG_MAX))) {
    const PRUnichar* tag = nsHTMLTags::GetStringValue(nodeType);
    PR_fprintf(mOutput, "\"%s\"", NS_ConvertUTF16toUTF8(tag).get());
  }
  else {
    char* text = nsnull;
    GetNewCString(aNode.GetText(), &text);
    if(text) {
      PR_fprintf(mOutput, "\"%s\"", text);
      nsMemory::Free(text);
    }
  }

  if (WillWriteAttributes(aNode)) {
    PR_fprintf(mOutput, ">\n");
    WriteAttributes(aNode);
    PR_fprintf(mOutput, "</open>\n");
  }
  else {
    PR_fprintf(mOutput, ">\n");
  }

  return NS_OK;
}

nsresult
nsLoggingSink::CloseNode(const char* aKind) {
	WriteTabs(mOutput,mLevel--);
  PR_fprintf(mOutput, "<close container=\"%s\">\n", aKind);
  return NS_OK;
}


nsresult
nsLoggingSink::WriteAttributes(const nsIParserNode& aNode) {

  WriteTabs(mOutput,1+mLevel);
  nsAutoString tmp;
  PRInt32 ac = aNode.GetAttributeCount();
  for (PRInt32 i = 0; i < ac; ++i) {
    char* key=nsnull;
    char* value=nsnull;
    const nsAString& k = aNode.GetKeyAt(i);
    const nsAString& v = aNode.GetValueAt(i);

    GetNewCString(k, &key);
    if(key) {
      PR_fprintf(mOutput, " <attr key=\"%s\" value=\"", key);
      nsMemory::Free(key);
    }
 
    tmp.Truncate();
    tmp.Append(v);
    if(!tmp.IsEmpty()) {
      PRUnichar first = tmp.First();
      if ((first == '"') || (first == '\'')) {
        if (tmp.Last() == first) {
          tmp.Cut(0, 1);
          PRInt32 pos = tmp.Length() - 1;
          if (pos >= 0) {
            tmp.Cut(pos, 1);
          }
        } else {
          // Mismatched quotes - leave them in
        }
      }
      GetNewCString(tmp, &value);

      if(value) {
        PR_fprintf(mOutput, "%s\"/>\n", value);
        WriteTabs(mOutput,1+mLevel);
        nsMemory::Free(value);
      }
    }
  }

  WriteTabs(mOutput,1+mLevel);
  return NS_OK;
}

PRBool
nsLoggingSink::WillWriteAttributes(const nsIParserNode& aNode)
{
  PRInt32 ac = aNode.GetAttributeCount();
  if (0 != ac) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

nsresult
nsLoggingSink::LeafNode(const nsIParserNode& aNode)
{
  WriteTabs(mOutput,1+mLevel);
  nsHTMLTag nodeType  = nsHTMLTag(aNode.GetNodeType());

  if ((nodeType >= eHTMLTag_unknown) &&
      (nodeType <= nsHTMLTag(NS_HTML_TAG_MAX))) {
    const PRUnichar* tag = nsHTMLTags::GetStringValue(nodeType);

    if(tag)
      PR_fprintf(mOutput, "<leaf tag=\"%s\"", NS_ConvertUTF16toUTF8(tag).get());
    else
      PR_fprintf(mOutput, "<leaf tag=\"???\"");

    if (WillWriteAttributes(aNode)) {
      PR_fprintf(mOutput, ">\n");
      WriteAttributes(aNode);
      PR_fprintf(mOutput, "</leaf>\n");
    }
    else {
      PR_fprintf(mOutput, "/>\n");
    }
  }
  else {
    PRInt32 pos;
    nsAutoString tmp;
    char* str = nsnull;
    switch (nodeType) {
    case eHTMLTag_whitespace:
    case eHTMLTag_text:
      GetNewCString(aNode.GetText(), &str);
      if(str) {
        PR_fprintf(mOutput, "<text value=\"%s\"/>\n", str);
        nsMemory::Free(str);
      }
      break;

    case eHTMLTag_newline:
      PR_fprintf(mOutput, "<newline/>\n");
      break;

    case eHTMLTag_entity:
      tmp.Append(aNode.GetText());
      tmp.Cut(0, 1);
      pos = tmp.Length() - 1;
      if (pos >= 0) {
        tmp.Cut(pos, 1);
      }
      PR_fprintf(mOutput, "<entity value=\"%s\"/>\n", NS_LossyConvertUTF16toASCII(tmp).get());
      break;

    default:
      NS_NOTREACHED("unsupported leaf node type");
    }//switch
  }
  return NS_OK;
}

nsresult 
nsLoggingSink::QuoteText(const nsAString& aValue, nsString& aResult) {
  aResult.Truncate();
    /*
      if you're stepping through the string anyway, why not use iterators instead of forcing the string to copy?
     */
  const nsPromiseFlatString& flat = PromiseFlatString(aValue);
  const PRUnichar* cp = flat.get();
  const PRUnichar* end = cp + aValue.Length();
  while (cp < end) {
    PRUnichar ch = *cp++;
    if (ch == '"') {
      aResult.AppendLiteral("&quot;");
    }
    else if (ch == '&') {
      aResult.AppendLiteral("&amp;");
    }
    else if ((ch < 32) || (ch >= 127)) {
      aResult.AppendLiteral("&#");
      aResult.AppendInt(PRInt32(ch), 10);
      aResult.Append(PRUnichar(';'));
    }
    else {
      aResult.Append(ch);
    }
  }
  return NS_OK;
}

/**
 * Use this method to convert nsString to char*. 
 * REMEMBER: Match this call with nsMemory::Free(aResult);
 * 
 * @update 04/04/99 harishd
 * @param aValue - The string value
 * @param aResult - String coverted to char*.
 */
nsresult
nsLoggingSink::GetNewCString(const nsAString& aValue, char** aResult)
{
  nsresult result=NS_OK;
  nsAutoString temp;
  result=QuoteText(aValue,temp);
  if(NS_SUCCEEDED(result)) {
    *aResult = temp.IsEmpty() ? nsnull : ToNewCString(temp);
  }
  return result;
}

/**
 * This gets called when handling illegal contents, especially
 * in dealing with tables. This method creates a new context.
 * 
 * @update 04/04/99 harishd
 * @param aPosition - The position from where the new context begins.
 */
NS_IMETHODIMP
nsLoggingSink::BeginContext(PRInt32 aPosition) 
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
nsLoggingSink::EndContext(PRInt32 aPosition)
{
  return NS_OK;
}
