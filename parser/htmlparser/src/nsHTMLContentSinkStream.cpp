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
 * @update  gess 4/1/98
 * 
 * This file declares the concrete HTMLContentSink class.
 * This class is used during the parsing process as the
 * primary interface between the parser and the content
 * model.
 */


#include "nsHTMLContentSinkStream.h"
#include "nsHTMLTokens.h"
#include <iostream.h>
#include <ctype.h>
#include "nsString.h"
#include "nsIParser.h"
#include "nsHTMLEntities.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIContentSinkIID, NS_ICONTENT_SINK_IID);
static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);

static char*          gHeaderComment = "<!-- This page was created by the Gecko output system. -->";
static char*          gDocTypeHeader = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">";
const  int            gTabSize=2;
static char           gBuffer[1024];

static const char* UnicodeToEntity(PRInt32 aCode);

static struct { char* mEntity; PRInt32 mValue; } entityTable[258] = {
  { "AElig", 198 }, { "AMP", 38 }, { "Aacute", 193 }, { "Acirc", 194 }, 
  { "Agrave", 192 }, { "Alpha", 913 }, { "Aring", 197 }, { "Atilde", 195 }, 
  { "Auml", 196 }, { "Beta", 914 }, { "COPY", 169 }, { "Ccedil", 199 }, 
  { "Chi", 935 }, { "Dagger", 8225 }, { "Delta", 916 }, { "ETH", 208 }, 
  { "Eacute", 201 }, { "Ecirc", 202 }, { "Egrave", 200 }, { "Epsilon", 917 }, 
  { "Eta", 919 }, { "Euml", 203 }, { "GT", 62 }, { "Gamma", 915 }, 
  { "Iacute", 205 }, { "Icirc", 206 }, { "Igrave", 204 }, { "Iota", 921 }, 
  { "Iuml", 207 }, { "Kappa", 922 }, { "LT", 60 }, { "Lambda", 923 }, 
  { "Mu", 924 }, { "Ntilde", 209 }, { "Nu", 925 }, { "OElig", 338 }, 
  { "Oacute", 211 }, { "Ocirc", 212 }, { "Ograve", 210 }, { "Omega", 937 }, 
  { "Omicron", 927 }, { "Oslash", 216 }, { "Otilde", 213 }, { "Ouml", 214 }, 
  { "Phi", 934 }, { "Pi", 928 }, { "Prime", 8243 }, { "Psi", 936 }, 
  { "QUOT", 34 }, { "REG", 174 }, { "Rho", 929 }, { "Scaron", 352 }, 
  { "Sigma", 931 }, { "THORN", 222 }, { "Tau", 932 }, { "Theta", 920 }, 
  { "Uacute", 218 }, { "Ucirc", 219 }, { "Ugrave", 217 }, { "Upsilon", 933 }, 
  { "Uuml", 220 }, { "Xi", 926 }, { "Yacute", 221 }, { "Yuml", 376 }, 
  { "Zeta", 918 }, { "aacute", 225 }, { "acirc", 226 }, { "acute", 180 }, 
  { "aelig", 230 }, { "agrave", 224 }, { "alefsym", 8501 }, { "alpha", 945 }, 
  { "amp", 38 }, { "and", 8743 }, { "ang", 8736 }, { "aring", 229 }, 
  { "asymp", 8776 }, { "atilde", 227 }, { "auml", 228 }, { "bdquo", 8222 }, 
  { "beta", 946 }, { "brvbar", 166 }, { "bull", 8226 }, { "cap", 8745 }, 
  { "ccedil", 231 }, { "cedil", 184 }, { "cent", 162 }, { "chi", 967 }, 
  { "circ", 710 }, { "clubs", 9827 }, { "cong", 8773 }, { "copy", 169 }, 
  { "crarr", 8629 }, { "cup", 8746 }, { "curren", 164 }, { "dArr", 8659 }, 
  { "dagger", 8224 }, { "darr", 8595 }, { "deg", 176 }, { "delta", 948 }, 
  { "diams", 9830 }, { "divide", 247 }, { "eacute", 233 }, { "ecirc", 234 }, 
  { "egrave", 232 }, { "empty", 8709 }, { "emsp", 8195 }, { "ensp", 8194 }, 
  { "epsilon", 949 }, { "equiv", 8801 }, { "eta", 951 }, { "eth", 240 }, 
  { "euml", 235 }, { "euro", 8364 }, { "exist", 8707 }, { "fnof", 402 }, 
  { "forall", 8704 }, { "frac12", 189 }, { "frac14", 188 }, 
  { "frac34", 190 }, { "frasl", 8260 }, { "gamma", 947 }, { "ge", 8805 }, 
  { "gt", 62 }, { "hArr", 8660 }, { "harr", 8596 }, { "hearts", 9829 }, 
  { "hellip", 8230 }, { "iacute", 237 }, { "icirc", 238 }, { "iexcl", 161 }, 
  { "igrave", 236 }, { "image", 8465 }, { "infin", 8734 }, { "int", 8747 }, 
  { "iota", 953 }, { "iquest", 191 }, { "isin", 8712 }, { "iuml", 239 }, 
  { "kappa", 954 }, { "lArr", 8656 }, { "lambda", 955 }, { "lang", 9001 }, 
  { "laquo", 171 }, { "larr", 8592 }, { "lceil", 8968 }, { "ldquo", 8220 }, 
  { "le", 8804 }, { "lfloor", 8970 }, { "lowast", 8727 }, { "loz", 9674 }, 
  { "lrm", 8206 }, { "lsaquo", 8249 }, { "lsquo", 8216 }, { "lt", 60 }, 
  { "macr", 175 }, { "mdash", 8212 }, { "micro", 181 }, { "middot", 183 }, 
  { "minus", 8722 }, { "mu", 956 }, { "nabla", 8711 }, { "nbsp", 160 }, 
  { "ndash", 8211 }, { "ne", 8800 }, { "ni", 8715 }, { "not", 172 }, 
  { "notin", 8713 }, { "nsub", 8836 }, { "ntilde", 241 }, { "nu", 957 }, 
  { "oacute", 243 }, { "ocirc", 244 }, { "oelig", 339 }, { "ograve", 242 }, 
  { "oline", 8254 }, { "omega", 969 }, { "omicron", 959 }, { "oplus", 8853 }, 
  { "or", 8744 }, { "ordf", 170 }, { "ordm", 186 }, { "oslash", 248 }, 
  { "otilde", 245 }, { "otimes", 8855 }, { "ouml", 246 }, { "para", 182 }, 
  { "part", 8706 }, { "permil", 8240 }, { "perp", 8869 }, { "phi", 966 }, 
  { "pi", 960 }, { "piv", 982 }, { "plusmn", 177 }, { "pound", 163 }, 
  { "prime", 8242 }, { "prod", 8719 }, { "prop", 8733 }, { "psi", 968 }, 
  { "quot", 34 }, { "rArr", 8658 }, { "radic", 8730 }, { "rang", 9002 }, 
  { "raquo", 187 }, { "rarr", 8594 }, { "rceil", 8969 }, { "rdquo", 8221 }, 
  { "real", 8476 }, { "reg", 174 }, { "rfloor", 8971 }, { "rho", 961 }, 
  { "rlm", 8207 }, { "rsaquo", 8250 }, { "rsquo", 8217 }, { "sbquo", 8218 }, 
  { "scaron", 353 }, { "sdot", 8901 }, { "sect", 167 }, { "shy", 173 }, 
  { "sigma", 963 }, { "sigmaf", 962 }, { "sim", 8764 }, { "spades", 9824 }, 
  { "sub", 8834 }, { "sube", 8838 }, { "sum", 8721 }, { "sup", 8835 }, 
  { "sup1", 185 }, { "sup2", 178 }, { "sup3", 179 }, { "supe", 8839 }, 
  { "szlig", 223 }, { "tau", 964 }, { "there4", 8756 }, { "theta", 952 }, 
  { "thetasym", 977 }, { "thinsp", 8201 }, { "thorn", 254 }, 
  { "tilde", 732 }, { "times", 215 }, { "trade", 8482 }, { "uArr", 8657 }, 
  { "uacute", 250 }, { "uarr", 8593 }, { "ucirc", 251 }, { "ugrave", 249 }, 
  { "uml", 168 }, { "upsih", 978 }, { "upsilon", 965 }, { "uuml", 252 }, 
  { "weierp", 8472 }, { "xi", 958 }, { "yacute", 253 }, { "yen", 165 }, 
  { "yuml", 255 }, { "zeta", 950 }, { "zwj", 8205 }, { "zwnj", 8204 }
};
#define NS_HTML_ENTITY_MAX 258







// XXX - WARNING, slow, we should have
// a much faster routine instead of scanning
// the entire list
static const char* UnicodeToEntity(PRInt32 aCode)
{
  for (PRInt32 i = 0; i < NS_HTML_ENTITY_MAX; i++)
  {
    if (entityTable[i].mValue == aCode)
      return entityTable[i].mEntity;
  }
  return nsnull;
}


 /** PRETTY PRINTING PROTOTYPES **/

class nsTagFormat
{
public:
  void Init(PRBool aBefore, PRBool aStart, PRBool aEnd, PRBool aAfter);
  void SetIndentGroup(PRUint8 aGroup);
  void SetFormat(PRBool aOnOff);

public:
  PRBool    mBreakBefore;
  PRBool    mBreakStart;
  PRBool    mBreakEnd;
  PRBool    mBreakAfter;
  
  PRUint8   mIndentGroup; // zero for none
  PRBool    mFormat;      // format (on|off)
};

void nsTagFormat::Init(PRBool aBefore, PRBool aStart, PRBool aEnd, PRBool aAfter)
{
  mBreakBefore = aBefore;
  mBreakStart = aStart;
  mBreakEnd = aEnd;
  mBreakAfter = aAfter;
  mFormat = PR_TRUE;
}

void nsTagFormat::SetIndentGroup(PRUint8 aGroup)
{
  mIndentGroup = aGroup;
}

void nsTagFormat::SetFormat(PRBool aOnOff)
{
  mFormat = aOnOff; 
}

class nsPrettyPrinter
{
public:

  void Init(PRBool aIndentEnable = PR_TRUE, PRUint8 aColSize = 2, PRUint8 aTabSize = 8, PRBool aUseTabs = PR_FALSE );    
  
  PRBool      mIndentEnable;
  PRUint8     mIndentColSize;
  PRUint8     mIndentTabSize;
  PRBool      mIndentUseTabs;

  PRBool      mAutowrapEnable;
  PRUint32    mAutoWrapColWidth;
  nsString    mBreak;             // CRLF, CR, LF

  nsTagFormat mTagFormat[NS_HTML_TAG_MAX+1];
};


void nsPrettyPrinter::Init(PRBool aIndentEnable, PRUint8 aColSize, PRUint8 aTabSize, PRBool aUseTabs)
{
  mIndentEnable = aIndentEnable;
  mIndentColSize = aColSize;
  mIndentTabSize = aTabSize;
  mIndentUseTabs = aUseTabs;

  mAutowrapEnable = PR_TRUE;
  mAutoWrapColWidth = 72;
  mBreak = "\n";              // CRLF, CR, LF

  for (PRUint32 i = 0; i < NS_HTML_TAG_MAX; i++)
    mTagFormat[i].Init(PR_FALSE,PR_FALSE,PR_FALSE,PR_FALSE);

  mTagFormat[eHTMLTag_a].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_abbr].Init(PR_FALSE,PR_FALSE,PR_FALSE,PR_FALSE);
  mTagFormat[eHTMLTag_applet].Init(PR_FALSE,PR_TRUE,PR_TRUE,PR_FALSE);
  mTagFormat[eHTMLTag_area].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_b].Init(PR_FALSE,PR_FALSE,PR_FALSE,PR_FALSE);
  mTagFormat[eHTMLTag_base].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_blockquote].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_body].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_br].Init(PR_FALSE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_caption].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_center].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_dd].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_dir].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_div].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_dl].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_dt].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_embed].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_form].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_frame].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_frameset].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_h1].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_h2].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_h3].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_h4].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_h5].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_h6].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_head].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_hr].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_html].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_ilayer].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_input].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_isindex].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_layer].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_li].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_link].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_map].Init(PR_FALSE,PR_TRUE,PR_TRUE,PR_FALSE);
  mTagFormat[eHTMLTag_menu].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_meta].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_object].Init(PR_FALSE,PR_TRUE,PR_TRUE,PR_FALSE);
  mTagFormat[eHTMLTag_ol].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_option].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_p].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_param].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_pre].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_script].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_select].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_style].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_table].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
  mTagFormat[eHTMLTag_td].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_textarea].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_th].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_title].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_tr].Init(PR_TRUE,PR_FALSE,PR_FALSE,PR_TRUE);
  mTagFormat[eHTMLTag_ul].Init(PR_TRUE,PR_TRUE,PR_TRUE,PR_TRUE);
}



 
static PRBool IsInline(eHTMLTags aTag);
static PRBool IsBlockLevel(eHTMLTags aTag);
static PRInt32 BreakBeforeOpen(eHTMLTags aTag);
static PRInt32 BreakAfterOpen(eHTMLTags aTag);
static PRInt32 BreakBeforeClose(eHTMLTags aTag);
static PRInt32 BreakAfterClose(eHTMLTags aTag);
static PRBool IndentChildren(eHTMLTags aTag);
static PRBool PreformattedChildren(eHTMLTags aTag);
static PRBool EatOpen(eHTMLTags aTag);
static PRBool EatClose(eHTMLTags aTag);
static PRBool PermitWSBeforeOpen(eHTMLTags aTag);
static PRBool PermitWSAfterOpen(eHTMLTags aTag);
static PRBool PermitWSBeforeClose(eHTMLTags aTag);
static PRBool PermitWSAfterClose(eHTMLTags aTag);
static PRBool IgnoreWS(eHTMLTags aTag);




/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gess 4/8/98
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 */
nsresult
nsHTMLContentSinkStream::QueryInterface(const nsIID& aIID, void** aInstancePtr)
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


NS_IMPL_ADDREF(nsHTMLContentSinkStream)
NS_IMPL_RELEASE(nsHTMLContentSinkStream)


/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsParser.
 *  
 *  @update  gess 4/8/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
NS_HTMLPARS nsresult
NS_New_HTML_ContentSinkStream(nsIHTMLContentSink** aInstancePtrResult, 
                              PRBool aDoFormat,
                              PRBool aDoHeader) {
  nsHTMLContentSinkStream* it = new nsHTMLContentSinkStream(aDoFormat,aDoHeader);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kIHTMLContentSinkIID, (void **)aInstancePtrResult);
}

/**
 * Construct a content sink stream.
 * @update	gess7/7/98
 * @param 
 * @return
 */
nsHTMLContentSinkStream::nsHTMLContentSinkStream(PRBool aDoFormat,PRBool aDoHeader)  {
  NS_INIT_REFCNT();
  mOutput=&cout;
  mLowerCaseTags = PR_TRUE;  
  memset(mHTMLTagStack,0,sizeof(mHTMLTagStack));
  mHTMLStackPos = 0;
  mColPos = 0;
  mIndent = 0;
  mDoFormat = aDoFormat;
  mDoHeader = aDoHeader;
  mBuffer = nsnull;
  mBufferSize = 0;
}

/**
 * Construct a content sink stream.
 * @update	gess7/7/98
 * @param 
 * @return
 */
nsHTMLContentSinkStream::nsHTMLContentSinkStream(ostream& aStream,PRBool aDoFormat,PRBool aDoHeader)  {
  NS_INIT_REFCNT();
  mOutput = &aStream;
  mLowerCaseTags = PR_TRUE;  
  memset(mHTMLTagStack,0,sizeof(mHTMLTagStack));
  mHTMLStackPos = 0;
  mColPos = 0;
  mIndent = 0;
  mDoFormat = aDoFormat;
  mDoHeader = aDoHeader;
  mBuffer = nsnull;
  mBufferSize = 0;
}


/**
 * This method tells the sink whether or not it is 
 * encoding an HTML fragment or the whole document.
 * By default, the entire document is encoded.
 *
 * @update 03/14/99 gpk
 * @param  aFlag set to true if only encoding a fragment
 */     
NS_IMETHODIMP
nsHTMLContentSinkStream::DoFragment(PRBool aFlag) 
{
  return NS_OK; 
}


void nsHTMLContentSinkStream::EnsureBufferSize(PRInt32 aNewSize)
{
  if (mBufferSize < aNewSize)
  {
    delete [] mBuffer;
    mBufferSize = 2*aNewSize+1; // make the twice as large
    mBuffer = new char[mBufferSize];
    mBuffer[0] = 0;
  }
}

void nsHTMLContentSinkStream::UnicodeToHTMLString(const nsString& aSrc)
{
  PRInt32       length = aSrc.Length();
  PRUnichar     ch; 
  const char*   entity = nsnull;
  PRUint32      offset = 0;
  PRUint32      addedLength = 0;

  if (length > 0)
  {
    EnsureBufferSize(length);
    for (PRInt32 i = 0; i < length; i++)
    {
      ch = aSrc[i];
      
      entity = UnicodeToEntity(ch);
      if (entity)
      {
        PRUint32 size = strlen(entity); 
        addedLength += size;
        EnsureBufferSize(length+addedLength+1);
        mBuffer[offset++] = '&';
        mBuffer[offset] = 0;
        strcat(mBuffer,entity);
        
        PRUint32 temp = offset + size;
        while (offset < temp)
        {
          mBuffer[offset] = tolower(mBuffer[offset]);
          offset++;
        }
        mBuffer[offset++] = ';';
        mBuffer[offset] = 0;
      }
      else if (ch < 128)
      {
        mBuffer[offset++] = (unsigned char)ch;
        mBuffer[offset] = 0;
      }
    }
  }
}


/**
 * 
 * @update	gess7/7/98
 * @param 
 * @return
 */
nsHTMLContentSinkStream::~nsHTMLContentSinkStream() {
  mOutput=0;  //we don't own the stream we're given; just forget it.
}


/**
 * 
 * @update	gess7/22/98
 * @param 
 * @return
 */
NS_IMETHODIMP_(void)
nsHTMLContentSinkStream::SetOutputStream(ostream& aStream){
  mOutput=&aStream;
}


/**
 * 
 * @update	gess7/7/98
 * @param 
 * @return
 */
void nsHTMLContentSinkStream::WriteAttributes(const nsIParserNode& aNode,ostream& aStream) {
  int theCount=aNode.GetAttributeCount();
  if(theCount) {
    int i=0;
    for(i=0;i<theCount;i++){
      const nsString& temp=aNode.GetKeyAt(i);
      
      if (!temp.Equals(nsString("Steve's unbelievable hack attribute")))
      {      
        nsString key = temp;

        if (mLowerCaseTags == PR_TRUE)
          key.ToLowerCase();
        else
          key.ToUpperCase();



        key.ToCString(mBuffer,sizeof(gBuffer)-1);
      
        aStream << " " << mBuffer << char(kEqual);
        mColPos += 1 + strlen(mBuffer) + 1;
      
        const nsString& value=aNode.GetValueAt(i);
        UnicodeToHTMLString(value);

        aStream << "\"" << mBuffer << "\"";
        mColPos += 1 + strlen(mBuffer) + 1;
      }
    }
  }
}


/**
 * 
 * @update	gess7/5/98
 * @param 
 * @return
 */
static
void OpenTagWithAttributes(const char* theTag,const nsIParserNode& aNode,int tab,ostream& aStream,PRBool aNewline) {
  int i=0;
  for(i=0;i<tab*gTabSize;i++) 
    aStream << " ";
  aStream << (char)kLessThan << theTag;
//  WriteAttributes(aNode,aStream);
  aStream << (char)kGreaterThan;
  if(aNewline)
    aStream << endl;
}


/**
 * 
 * @update	gess7/5/98
 * @param 
 * @return
 */
static
void OpenTag(const char* theTag,int tab,ostream& aStream,PRBool aNewline) {
  int i=0;
  for(i=0;i<tab*gTabSize;i++) 
    aStream << " ";
  aStream << (char)kLessThan << theTag << (char)kGreaterThan;
  if(aNewline)
    aStream << endl;
}


/**
 * 
 * @update	gess7/5/98
 * @param 
 * @return
 */
static
void CloseTag(const char* theTag,int tab,ostream& aStream) {
  int i=0;
  for(i=0;i<tab*gTabSize;i++) 
    aStream << " ";
  aStream << (char)kLessThan << (char)kForwardSlash << theTag << (char)kGreaterThan << endl;
}


/**
 * 
 * @update	gess7/5/98
 * @param 
 * @return
 */
static
void WritePair(eHTMLTags aTag,const nsString& theContent,int tab,ostream& aStream) {
  const char* titleStr = GetTagName(aTag);
  OpenTag(titleStr,tab,aStream,PR_FALSE);  
  theContent.ToCString(gBuffer,sizeof(gBuffer)-1);
  aStream << gBuffer;
  CloseTag(titleStr,0,aStream);
}

/**
  * This method gets called by the parser when it encounters
  * a title tag and wants to set the document title in the sink.
  *
  * @update 4/1/98 gess
  * @param  nsString reference to new title value
  * @return PR_TRUE if successful. 
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::SetTitle(const nsString& aValue){
  if(mOutput) {
    WritePair(eHTMLTag_title,aValue,1+mTabLevel,*mOutput);
  }
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
nsHTMLContentSinkStream::OpenHTML(const nsIParserNode& aNode){
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_html)
     AddStartTag(aNode,*mOutput);
  }
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
nsHTMLContentSinkStream::CloseHTML(const nsIParserNode& aNode){
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_html)
      AddEndTag(aNode,*mOutput);
      mOutput->flush();
  }
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
nsHTMLContentSinkStream::OpenHead(const nsIParserNode& aNode){
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_head)
     AddStartTag(aNode,*mOutput);
  }
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
nsHTMLContentSinkStream::CloseHead(const nsIParserNode& aNode){
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_head)
      AddEndTag(aNode,*mOutput);
  }
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
nsHTMLContentSinkStream::OpenBody(const nsIParserNode& aNode){
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_body)
      AddStartTag(aNode,*mOutput);
  }
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
nsHTMLContentSinkStream::CloseBody(const nsIParserNode& aNode){
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_body)
      AddEndTag(aNode,*mOutput);
  }
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
nsHTMLContentSinkStream::OpenForm(const nsIParserNode& aNode){
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_form)
      AddStartTag(aNode,*mOutput);
  }
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
nsHTMLContentSinkStream::CloseForm(const nsIParserNode& aNode){
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_form)
      AddEndTag(aNode,*mOutput);
  }
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
nsHTMLContentSinkStream::OpenMap(const nsIParserNode& aNode){
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_map)
      AddStartTag(aNode,*mOutput);
  }
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
nsHTMLContentSinkStream::CloseMap(const nsIParserNode& aNode){
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_map)
      AddEndTag(aNode,*mOutput);
  }
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
nsHTMLContentSinkStream::OpenFrameset(const nsIParserNode& aNode){
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_frameset)
      AddStartTag(aNode,*mOutput);
  }
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
nsHTMLContentSinkStream::CloseFrameset(const nsIParserNode& aNode){
  if(mOutput) {
    eHTMLTags tag = (eHTMLTags)aNode.GetNodeType();
    if (tag == eHTMLTag_frameset)
      AddEndTag(aNode,*mOutput);
  }
  return NS_OK;
}


void nsHTMLContentSinkStream::AddIndent(ostream& aStream)
{
  for (PRInt32 i = mIndent; --i >= 0; ) 
  {
    aStream << "  ";
    mColPos += 2;
  }
}



void nsHTMLContentSinkStream::AddStartTag(const nsIParserNode& aNode, ostream& aStream)
{
  eHTMLTags         tag = (eHTMLTags)aNode.GetNodeType();
  const nsString&   name = aNode.GetText();
  nsString          tagName;

  mHTMLTagStack[mHTMLStackPos++] = tag;
  tagName = name;
  
  if (mLowerCaseTags == PR_TRUE)
    tagName.ToLowerCase();
  else
    tagName.ToUpperCase();

  
  if (mColPos != 0 && BreakBeforeOpen(tag))
  {
    aStream << endl;
    mColPos = 0;
  }

  if (PermitWSBeforeOpen(tag))
    AddIndent(aStream);

  tagName.ToCString(gBuffer,sizeof(gBuffer)-1);
  aStream << (char)kLessThan << gBuffer;
  mColPos += 1 + tagName.Length();

  if (tag == eHTMLTag_style)
  {
    aStream << (char)kGreaterThan << endl;
    const   nsString& data = aNode.GetSkippedContent();
    PRInt32 size = data.Length();
    char*   buffer = new char[size+1];
    data.ToCString(buffer,size+1);
    aStream << buffer;
    delete buffer;
  }
  else
  {
    WriteAttributes(aNode,aStream);
    aStream << (char)kGreaterThan;
    mColPos += 1;
  }

  if (BreakAfterOpen(tag))
  {
    aStream << endl;
    mColPos = 0;
  }

  if (IndentChildren(tag))
    mIndent++;
}




void nsHTMLContentSinkStream::AddEndTag(const nsIParserNode& aNode, ostream& aStream)
{
  eHTMLTags         tag = (eHTMLTags)aNode.GetNodeType();
//  const nsString&   name = aNode.GetText();
  nsString          tagName;

  if (tag == eHTMLTag_unknown)
  {
    tagName = aNode.GetText();
  }
  else
  {
    const char*  name =  NS_EnumToTag(tag);
    tagName = name;
  }
  if (mLowerCaseTags == PR_TRUE)
    tagName.ToLowerCase();
  else
    tagName.ToUpperCase();

  if (IndentChildren(tag))
    mIndent--;

  if (BreakBeforeClose(tag))
  {
    if (mColPos != 0)
    {
      aStream << endl;
      mColPos = 0;
    }
    AddIndent(aStream);
  }

  tagName.ToCString(gBuffer,sizeof(gBuffer)-1);
  aStream << (char)kLessThan << (char)kForwardSlash << gBuffer << (char)kGreaterThan;
  mColPos += 1 + 1 + strlen(gBuffer) + 1;
  
  if (BreakAfterClose(tag))
  {
    aStream << endl;
    mColPos = 0;
  }
  mHTMLTagStack[--mHTMLStackPos] = eHTMLTag_unknown;
}



/**
 *  This gets called by the parser when you want to add
 *  a leaf node to the current container in the content
 *  model.
 *  
 *  @updated gpk 06/18/98
 *  @param   
 *  @return  
 */
nsresult
nsHTMLContentSinkStream::AddLeaf(const nsIParserNode& aNode, ostream& aStream){
  eHTMLTags type = (eHTMLTags)aNode.GetNodeType();
  eHTMLTags tag = eHTMLTag_unknown;
  if (mHTMLStackPos > 0)
    tag = mHTMLTagStack[mHTMLStackPos-1];
  
  PRBool    preformatted = PR_FALSE;



  for (PRInt32 i = mHTMLStackPos-1; i >= 0; i--)
  {
    preformatted |= PreformattedChildren(mHTMLTagStack[i]);
    if (preformatted)
      break;
  }

  if (type == eHTMLTag_br ||
      type == eHTMLTag_hr ||
      type == eHTMLTag_meta || 
      type == eHTMLTag_style)
  {
    AddStartTag(aNode,aStream);
    mHTMLTagStack[--mHTMLStackPos] = eHTMLTag_unknown;
  }
  if (type == eHTMLTag_text)
  {
    const nsString& text = aNode.GetText();
    if ((mDoFormat == PR_FALSE) || preformatted == PR_TRUE)
    {
      UnicodeToHTMLString(text);
      aStream << mBuffer;
      mColPos += text.Length();
    }
    else
    {
      PRInt32 mMaxColumn = 72;

      // 1. Determine the length of the input string
      PRInt32 length = text.Length();

      // 2. If the offset plus the length of the text is smaller
      // than the max then just add it 
      if (mColPos + length < mMaxColumn)
      {
        UnicodeToHTMLString(text);
        aStream << mBuffer;
        mColPos += text.Length();
      }
      else
      {
        nsString  str = text;
        PRBool    done = PR_FALSE;
        PRInt32   index = 0;
        PRInt32   offset = mColPos;

        while (!done)
        {        
          // find the next break
          PRInt32 start = mMaxColumn-offset;
          if (start < 0)
            start = 0;
          
          index = str.Find(' ',start);

          // if there is no break than just add it
          if (index == kNotFound)
          {
            UnicodeToHTMLString(str);
            aStream << mBuffer;
            mColPos += str.Length();
            done = PR_TRUE;
          }
          else
          {
            // make first equal to the str from the 
            // beginning to the index
            nsString  first = str;

            first.Truncate(index);
  
            UnicodeToHTMLString(first);
            aStream << mBuffer << endl;
            mColPos = 0;
  
            // cut the string from the beginning to the index
            str.Cut(0,index);
            offset = 0;
          }
        }
      }
    }
  }
  else if (type == eHTMLTag_whitespace)
  {
    if ((mDoFormat == PR_FALSE) || preformatted || IgnoreWS(tag) == PR_FALSE)
    {
      const nsString& text = aNode.GetText();
      UnicodeToHTMLString(text);
      aStream << mBuffer;
      mColPos += text.Length();
    }
  }
  else if (type == eHTMLTag_newline)
  {
    if ((mDoFormat == PR_FALSE) || preformatted)
    {
      const nsString& text = aNode.GetText();
      UnicodeToHTMLString(text);
      aStream << mBuffer;
      mColPos = 0;
    }
  }

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
nsHTMLContentSinkStream::AddProcessingInstruction(const nsIParserNode& aNode){

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
nsHTMLContentSinkStream::AddComment(const nsIParserNode& aNode){

#ifdef VERBOSE_DEBUG
  DebugDump("<",aNode.GetText(),(mNodeStackPos)*2);
#endif

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
nsHTMLContentSinkStream::OpenContainer(const nsIParserNode& aNode){
  if(mOutput) {
    AddStartTag(aNode,*mOutput);
//    eHTMLTags  tag = (eHTMLTags)aNode.GetNodeType();
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
nsHTMLContentSinkStream::CloseContainer(const nsIParserNode& aNode){
  if(mOutput) {
    AddEndTag(aNode,*mOutput);
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
nsHTMLContentSinkStream::AddLeaf(const nsIParserNode& aNode){
  nsresult result = NS_OK;
  if(mOutput) {  
    result = AddLeaf(aNode,*mOutput);
  }
  return result;
}


/**
  * This method gets called when the parser begins the process
  * of building the content model via the content sink.
  *
  * @update 5/7/98 gess
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::WillBuildModel(void){
  mTabLevel=-1;
  if(mDoHeader && mOutput) {
    (*mOutput) << gHeaderComment << endl;
    (*mOutput) << gDocTypeHeader << endl;
  }
  return NS_OK;
}


/**
  * This method gets called when the parser concludes the process
  * of building the content model via the content sink.
  *
  * @param  aQualityLevel describes how well formed the doc was.
  *         0=GOOD; 1=FAIR; 2=POOR;
  * @update 5/7/98 gess
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::DidBuildModel(PRInt32 aQualityLevel) {
  return NS_OK;
}


/**
  * This method gets called when the parser gets i/o blocked,
  * and wants to notify the sink that it may be a while before
  * more data is available.
  *
  * @update 5/7/98 gess
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::WillInterrupt(void) {
  return NS_OK;
}


/**
  * This method gets called when the parser i/o gets unblocked,
  * and we're about to start dumping content again to the sink.
  *
  * @update 5/7/98 gess
  */     
NS_IMETHODIMP
nsHTMLContentSinkStream::WillResume(void) {
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContentSinkStream::SetParser(nsIParser* aParser) {
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLContentSinkStream::NotifyError(const nsParserError* aError)
{
  return NS_OK;
}


/**
  * **** Pretty Printing Methods ******
  *
  */
 


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


/**
  * Desired line break state before the open tag.
  */
PRBool BreakBeforeOpen(eHTMLTags aTag)  {
 PRBool  result = PR_FALSE;
  switch (aTag)
  {
    case  eHTMLTag_html:
      result = PR_FALSE;
    break;

    default:
      result = IsBlockLevel(aTag);
  }
  return result;
}

/**
  * Desired line break state after the open tag.
  */
PRBool BreakAfterOpen(eHTMLTags aTag)  {
  PRBool  result = PR_FALSE;
  switch (aTag)
  {
    case eHTMLTag_html:
    case eHTMLTag_body:
    case eHTMLTag_ul:
    case eHTMLTag_ol:
    case eHTMLTag_table:
    case eHTMLTag_tbody:
    case eHTMLTag_style:
      result = PR_TRUE;
      break;

    default:
      break;

  }
  return result;
}

/**
  * Desired line break state before the close tag.
  */
PRBool BreakBeforeClose(eHTMLTags aTag)  {
  PRBool  result = PR_FALSE;

  switch (aTag)
  {
    case eHTMLTag_html:
    case eHTMLTag_head:
    case eHTMLTag_body:
    case eHTMLTag_ul:
    case eHTMLTag_ol:
    case eHTMLTag_table:
    case eHTMLTag_tbody:
    case eHTMLTag_style:
      result = PR_TRUE;
      break;

    default:
      break;

  }
  return result;
}

/**
  * Desired line break state after the close tag.
  */
PRBool BreakAfterClose(eHTMLTags aTag)  {
  PRBool  result = PR_FALSE;

  switch (aTag)
  {
    case  eHTMLTag_html:
      result = PR_TRUE;
    break;

    default:
      result = IsBlockLevel(aTag);
  }
  return result;
}

/**
  * Indent/outdent when the open/close tags are encountered.
  * This implies that BreakAfterOpen() and BreakBeforeClose()
  * are true no matter what those methods return.
  */
PRBool IndentChildren(eHTMLTags aTag)  {
  
  PRBool result = PR_FALSE;

  switch (aTag)
  {
    case eHTMLTag_table:
    case eHTMLTag_ul:
    case eHTMLTag_ol:
    case eHTMLTag_tbody:
    case eHTMLTag_form:
    case eHTMLTag_frameset:
      result = PR_TRUE;      
      break;

    default:
      result = PR_FALSE;
      break;
  }
  return result;  
}

/**
  * All tags after this tag and before the closing tag will be output with no
  * formatting.
  */
PRBool PreformattedChildren(eHTMLTags aTag)  {
  PRBool result = PR_FALSE;
  if (aTag == eHTMLTag_pre)
  {
    result = PR_TRUE;
  }
  return result;
}

/**
  * Eat the open tag.  Pretty much just for <P*>.
  */
PRBool EatOpen(eHTMLTags aTag)  {
  return PR_FALSE;
}

/**
  * Eat the close tag.  Pretty much just for </P>.
  */
PRBool EatClose(eHTMLTags aTag)  {
  return PR_FALSE;
}

/**
  * Are we allowed to insert new white space before the open tag.
  *
  * Returning false does not prevent inserting WS
  * before the tag if WS insertion is allowed for another reason,
  * e.g. there is already WS there or we are after a tag that
  * has PermitWSAfter*().
  */
PRBool PermitWSBeforeOpen(eHTMLTags aTag)  {
  PRBool result = IsInline(aTag) == PR_FALSE;
  return result;
}

/** @see PermitWSBeforeOpen */
PRBool PermitWSAfterOpen(eHTMLTags aTag)  {
  if (aTag == eHTMLTag_pre)
  {
    return PR_FALSE;
  }
  return PR_TRUE;
}

/** @see PermitWSBeforeOpen */
PRBool PermitWSBeforeClose(eHTMLTags aTag)  {
  if (aTag == eHTMLTag_pre)
  {
    return PR_FALSE;
  }
  return PR_TRUE;
}

/** @see PermitWSBeforeOpen */
PRBool PermitWSAfterClose(eHTMLTags aTag)  {
  return PR_TRUE;
}


/** @see PermitWSBeforeOpen */
PRBool IgnoreWS(eHTMLTags aTag)  {
  PRBool result = PR_FALSE;

  switch (aTag)
  {
    case eHTMLTag_html:
    case eHTMLTag_head:
    case eHTMLTag_body:
    case eHTMLTag_ul:
    case eHTMLTag_ol:
    case eHTMLTag_li:
    case eHTMLTag_table:
    case eHTMLTag_tbody:
    case eHTMLTag_style:
      result = PR_TRUE;
      break;
    default:
      break;
  }

  return result;
}


