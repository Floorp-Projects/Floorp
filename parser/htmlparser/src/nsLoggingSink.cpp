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
#include "nsILoggingSink.h"
#include "nsHTMLTags.h"
#include "nsString.h"

static NS_DEFINE_IID(kIContentSinkIID, NS_ICONTENT_SINK_IID);
static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);
static NS_DEFINE_IID(kILoggingSinkIID, NS_ILOGGING_SINK_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

// list of tags that have skipped content
static char gSkippedContentTags[] = {
  eHTMLTag_style,
  eHTMLTag_script,
  eHTMLTag_server,
  eHTMLTag_textarea,
  eHTMLTag_title,
  0
};

/**
 * 
 * @update	gess8/8/98
 * @param 
 * @return
 */
ostream& operator<<(ostream& os, nsAutoString& aString) {
	const PRUnichar* uc=aString.GetUnicode();
	int len=aString.Length();
	for(int i=0;i<len;i++)
		os<<(char)uc[i];
	return os;
}

/**
 * 
 * @update	gess8/8/98
 * @param 
 * @return
 */
ostream& operator<<(ostream& os,const nsString& aString) {
	const PRUnichar* uc=aString.GetUnicode();
	int len=aString.Length();
	for(int i=0;i<len;i++)
		os<<(char)uc[i];
	return os;
}

class nsLoggingSink : public nsILoggingSink {
public:
  nsLoggingSink();
  virtual ~nsLoggingSink();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIContentSink
  NS_IMETHOD WillBuildModel();
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
  NS_IMETHOD WillInterrupt();
  NS_IMETHOD WillResume();
  NS_IMETHOD SetParser(nsIParser* aParser);
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD NotifyError(const nsParserError* aError);
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);

  // nsIHTMLContentSink
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
  NS_IMETHOD DoFragment(PRBool aFlag);
  NS_IMETHOD BeginContext(PRInt32 aPosition);
  NS_IMETHOD EndContext(PRInt32 aPosition);

  // nsILoggingSink
  NS_IMETHOD SetOutputStream(ostream& aStream);

  nsresult OpenNode(const char* aKind, const nsIParserNode& aNode);
  nsresult CloseNode(const char* aKind);
  nsresult LeafNode(const nsIParserNode& aNode);
  nsresult WriteAttributes(const nsIParserNode& aNode);
  nsresult QuoteText(const nsString& aValue, nsString& aResult);
  PRBool WillWriteAttributes(const nsIParserNode& aNode);

protected:
	ostream*	mOutput;
	int				mLevel;
};

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

nsLoggingSink::nsLoggingSink()
{
  NS_INIT_REFCNT();
  mOutput = 0;
	mLevel=-1;
}

nsLoggingSink::~nsLoggingSink()
{
  if (0 != mOutput) {
    mOutput->flush();  
    mOutput = 0;
  }
}

NS_IMPL_ADDREF(nsLoggingSink)
NS_IMPL_RELEASE(nsLoggingSink)

nsresult
nsLoggingSink::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(nsnull != aInstancePtr, "null ptr");
  if (nsnull == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {
    nsISupports* tmp = this;
    *aInstancePtr = (void*) tmp;
  }
  else if (aIID.Equals(kIContentSinkIID)) {
    nsIContentSink* tmp = this;
    *aInstancePtr = (void*) tmp;
  }
  else if (aIID.Equals(kIHTMLContentSinkIID)) {
    nsIHTMLContentSink* tmp = this;
    *aInstancePtr = (void*) tmp;
  }
  else if (aIID.Equals(kILoggingSinkIID)) {
    nsILoggingSink* tmp = this;
    *aInstancePtr = (void*) tmp;
  }
  else {
    *aInstancePtr = nsnull;
    return NS_NOINTERFACE;
  }
  NS_ADDREF(this);
  return NS_OK;
}

NS_IMETHODIMP
nsLoggingSink::SetOutputStream(ostream& aStream) {
  mOutput = &aStream;
  return NS_OK;
}

static
void WriteTabs(ostream& anOutputStream,int aTabCount) {
	int tabs;
	for(tabs=0;tabs<aTabCount;tabs++)
		anOutputStream << "  ";
}


NS_IMETHODIMP
nsLoggingSink::WillBuildModel()
{
	WriteTabs(*mOutput,++mLevel);
	(*mOutput) << "<begin>" << endl;
  return NS_OK;
}

NS_IMETHODIMP
nsLoggingSink::DidBuildModel(PRInt32 aQualityLevel)
{
	WriteTabs(*mOutput,mLevel--);
	(*mOutput) << "</begin>" << endl;
  return NS_OK;
}

NS_IMETHODIMP
nsLoggingSink::WillInterrupt()
{
  return NS_OK;
}

NS_IMETHODIMP
nsLoggingSink::WillResume()
{
  return NS_OK;
}

NS_IMETHODIMP
nsLoggingSink::SetParser(nsIParser* aParser) 
{
  return NS_OK;
}

NS_IMETHODIMP
nsLoggingSink::OpenContainer(const nsIParserNode& aNode)
{
  return OpenNode("container", aNode);
}

NS_IMETHODIMP
nsLoggingSink::CloseContainer(const nsIParserNode& aNode)
{

  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
  if ((nodeType >= eHTMLTag_unknown) &&
      (nodeType <= nsHTMLTag(NS_HTML_TAG_MAX))) {
    const char* tag = NS_EnumToTag(nodeType);
		return CloseNode(tag);
	}
	return CloseNode("???");
}

NS_IMETHODIMP
nsLoggingSink::AddLeaf(const nsIParserNode& aNode)
{
  return LeafNode(aNode);
}

NS_IMETHODIMP 
nsLoggingSink::NotifyError(const nsParserError* aError)
{
  return NS_OK;
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

  return NS_OK;
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

  return NS_OK;
}


NS_IMETHODIMP
nsLoggingSink::SetTitle(const nsString& aValue)
{
  nsAutoString tmp;
  QuoteText(aValue, tmp);
	WriteTabs(*mOutput,++mLevel);
	(*mOutput) << "<title value=\"" << tmp << "\"/>" << endl;
	--mLevel;
  return NS_OK;
}

NS_IMETHODIMP
nsLoggingSink::OpenHTML(const nsIParserNode& aNode)
{
  return OpenNode("html", aNode);
}

NS_IMETHODIMP
nsLoggingSink::CloseHTML(const nsIParserNode& aNode)
{
  return CloseNode("html");
}

NS_IMETHODIMP
nsLoggingSink::OpenHead(const nsIParserNode& aNode)
{
  return OpenNode("head", aNode);
}

NS_IMETHODIMP
nsLoggingSink::CloseHead(const nsIParserNode& aNode)
{
  return CloseNode("head");
}

NS_IMETHODIMP
nsLoggingSink::OpenBody(const nsIParserNode& aNode)
{
  return OpenNode("body", aNode);
}

NS_IMETHODIMP
nsLoggingSink::CloseBody(const nsIParserNode& aNode)
{
  return CloseNode("body");
}

NS_IMETHODIMP
nsLoggingSink::OpenForm(const nsIParserNode& aNode)
{
  return OpenNode("form", aNode);
}

NS_IMETHODIMP
nsLoggingSink::CloseForm(const nsIParserNode& aNode)
{
  return CloseNode("form");
}

NS_IMETHODIMP
nsLoggingSink::OpenMap(const nsIParserNode& aNode)
{
  return OpenNode("map", aNode);
}

NS_IMETHODIMP
nsLoggingSink::CloseMap(const nsIParserNode& aNode)
{
  return CloseNode("map");
}

NS_IMETHODIMP
nsLoggingSink::OpenFrameset(const nsIParserNode& aNode)
{
  return OpenNode("frameset", aNode);
}

NS_IMETHODIMP
nsLoggingSink::CloseFrameset(const nsIParserNode& aNode)
{
  return CloseNode("frameset");
}


nsresult
nsLoggingSink::OpenNode(const char* aKind, const nsIParserNode& aNode)
{
	WriteTabs(*mOutput,++mLevel);

	(*mOutput) << "<open container=";

  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
  if ((nodeType >= eHTMLTag_unknown) &&
      (nodeType <= nsHTMLTag(NS_HTML_TAG_MAX))) {
    const char* tag = NS_EnumToTag(nodeType);
		(*mOutput) << "\"" << tag << "\"";
  }
  else {
    const nsString& text = aNode.GetText();
		(*mOutput) << "\"" << text << " \"";
  }

  if (WillWriteAttributes(aNode)) {
		(*mOutput) << ">" << endl;
    WriteAttributes(aNode);
		(*mOutput) << "</open>" << endl;
  }
  else {
		(*mOutput) << ">" << endl;
  }

  return NS_OK;
}

nsresult
nsLoggingSink::CloseNode(const char* aKind)
{
	WriteTabs(*mOutput,mLevel--);
	(*mOutput) << "<close container=\"" << aKind << "\">" << endl;
  return NS_OK;
}


nsresult
nsLoggingSink::WriteAttributes(const nsIParserNode& aNode)
{
  nsAutoString tmp, tmp2;
  PRInt32 ac = aNode.GetAttributeCount();
  for (PRInt32 i = 0; i < ac; i++) {
    const nsString& k = aNode.GetKeyAt(i);
    const nsString& v = aNode.GetValueAt(i);

		(*mOutput) << " <attr key=\"" << k << "\" value=\"";
 
    tmp.Truncate();
    tmp.Append(v);
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
    QuoteText(tmp, tmp2);

		(*mOutput) << tmp2 << "\"/>" << endl;
  }

  if (0 != strchr(gSkippedContentTags, aNode.GetNodeType())) {
    const nsString& content = aNode.GetSkippedContent();
    if (content.Length() > 0) {
      nsAutoString tmp;

      QuoteText(content, tmp);
			(*mOutput) << " <content value=\"";
			(*mOutput) << tmp << "\"/>" << endl;
    }
  }

  return NS_OK;
}

PRBool
nsLoggingSink::WillWriteAttributes(const nsIParserNode& aNode)
{
  PRInt32 ac = aNode.GetAttributeCount();
  if (0 != ac) {
    return PR_TRUE;
  }
  if (0 != strchr(gSkippedContentTags, aNode.GetNodeType())) {
    const nsString& content = aNode.GetSkippedContent();
    if (content.Length() > 0) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

nsresult
nsLoggingSink::LeafNode(const nsIParserNode& aNode)
{
 	WriteTabs(*mOutput,1+mLevel);
	nsHTMLTag				nodeType  = nsHTMLTag(aNode.GetNodeType());

  if ((nodeType >= eHTMLTag_unknown) &&
      (nodeType <= nsHTMLTag(NS_HTML_TAG_MAX))) {
    const char* tag = NS_EnumToTag(nodeType);

		if(tag)
			(*mOutput) << "<leaf tag=\"" << tag << "\"";
		else (*mOutput) << "<leaf tag=\"???\"";

    if (WillWriteAttributes(aNode)) {
			(*mOutput) << ">" << endl;
      WriteAttributes(aNode);
			(*mOutput) << "</leaf>" << endl;
    }
    else {
			(*mOutput) << "/>" << endl;
    }
  }
  else {
    PRInt32 pos;
    nsAutoString tmp;
    switch (nodeType) {
			case eHTMLTag_whitespace:
			case eHTMLTag_text:
				QuoteText(aNode.GetText(), tmp);
				(*mOutput) << "<text value=\"" << tmp << "\"/>" << endl;
				break;

			case eHTMLTag_newline:
				(*mOutput) << "<newline/>" << endl;
				break;

			case eHTMLTag_entity:
				tmp.Append(aNode.GetText());
				tmp.Cut(0, 1);
				pos = tmp.Length() - 1;
				if (pos >= 0) {
					tmp.Cut(pos, 1);
				}
				(*mOutput) << "<entity value=\"" << tmp << "\"/>" << endl;
				break;

			default:
				NS_NOTREACHED("unsupported leaf node type");
		}//switch
  }
  return NS_OK;
}

nsresult
nsLoggingSink::QuoteText(const nsString& aValue, nsString& aResult)
{
  aResult.Truncate();
  const PRUnichar* cp = aValue.GetUnicode();
  const PRUnichar* end = cp + aValue.Length();
  while (cp < end) {
    PRUnichar ch = *cp++;
    if (ch == '"') {
      aResult.Append("&quot;");
    }
    else if (ch == '&') {
      aResult.Append("&amp;");
    }
    else if ((ch < 32) || (ch >= 127)) {
      aResult.Append("&#");
      aResult.Append(PRInt32(ch), 10);
      aResult.Append(';');
    }
    else {
      aResult.Append(ch);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsLoggingSink::DoFragment(PRBool aFlag) 
{
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
