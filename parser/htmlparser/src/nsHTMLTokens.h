/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 *  
 * This file contains the declarations for all the 
 * HTML specific token types that our HTML tokenizer
 * delegate understands. 
 *
 * If you want to add a new kind of token, this is 
 * the place to do it. You should also add a bit of glue
 * code to the HTML tokenizer delegate class.
 */

#ifndef HTMLTOKENS_H
#define HTMLTOKENS_H

#include "nsToken.h"
#include <iostream.h>


class CScanner;

enum eHTMLTokenTypes {
  
  eToken_unknown=0,
  eToken_start=1,     eToken_end,     eToken_comment,         eToken_entity,
  eToken_whitespace,  eToken_newline, eToken_text,            eToken_attribute,
  eToken_script,      eToken_style,   eToken_skippedcontent,  
  eToken_last //make sure this stays the last token...
};

//*** This enum is used to define the known universe of HTML tags.
//*** The use of this table doesn't preclude of from using non-standard
//*** tags. It simply makes normal tag handling more efficient.
enum eHTMLTags
{
  eHTMLTag_unknown=0,   eHTMLTag_doctype,     eHTMLTag_a,         eHTMLTag_acronym,
  eHTMLTag_address,     eHTMLTag_applet,      eHTMLTag_area,      eHTMLTag_bold,
  eHTMLTag_base,        eHTMLTag_basefont,    eHTMLTag_bdo,       eHTMLTag_big,         
  eHTMLTag_blink,       eHTMLTag_blockquote,  eHTMLTag_body,      eHTMLTag_br,          
  eHTMLTag_button,      eHTMLTag_caption,     eHTMLTag_center,    
  eHTMLTag_certificate, eHTMLTag_cite,
  eHTMLTag_code,        eHTMLTag_col,         eHTMLTag_colgroup,  eHTMLTag_comment,
  eHTMLTag_dd,          eHTMLTag_del,         eHTMLTag_dfn,       eHTMLTag_div,       
  eHTMLTag_dir,         eHTMLTag_dl,          eHTMLTag_dt,        
  eHTMLTag_em,          eHTMLTag_embed,
  eHTMLTag_fieldset,    eHTMLTag_font,        eHTMLTag_footer,  
  eHTMLTag_form,        eHTMLTag_frame,       eHTMLTag_frameset,
  eHTMLTag_h1,          eHTMLTag_h2,          eHTMLTag_h3,        eHTMLTag_h4,
  eHTMLTag_h5,          eHTMLTag_h6,          eHTMLTag_head,      eHTMLTag_header,
  eHTMLTag_hr,          eHTMLTag_html,        eHTMLTag_iframe,    eHTMLTag_ilayer,
  eHTMLTag_italic,      eHTMLTag_img,         eHTMLTag_ins,       eHTMLTag_input,       
  eHTMLTag_isindex,       
  eHTMLTag_kbd,         eHTMLTag_keygen,
  eHTMLTag_label,       eHTMLTag_layer,       eHTMLTag_legend,    eHTMLTag_listitem,
  eHTMLTag_link,        eHTMLTag_listing,     eHTMLTag_map,       eHTMLTag_marquee,
  eHTMLTag_math,        eHTMLTag_menu,        eHTMLTag_meta,      eHTMLTag_newline,
  eHTMLTag_noembed,     eHTMLTag_noframes,    eHTMLTag_nolayer,   eHTMLTag_noscript,  
  eHTMLTag_note,        eHTMLTag_object,      eHTMLTag_ol,
  eHTMLTag_option,      eHTMLTag_paragraph,   eHTMLTag_param,     eHTMLTag_plaintext,   
  eHTMLTag_pre,         eHTMLTag_quotation,   eHTMLTag_strike,    eHTMLTag_samp,        
  eHTMLTag_script,      eHTMLTag_select,      
  eHTMLTag_server,      eHTMLTag_small,     
  eHTMLTag_spacer,      eHTMLTag_span,
  eHTMLTag_strong,      eHTMLTag_style,       eHTMLTag_sub,       eHTMLTag_sup,         
  eHTMLTag_table,       eHTMLTag_tbody,       eHTMLTag_td,        
  
  eHTMLTag_text,  //used for plain text; this is not really a tag.   
  eHTMLTag_textarea,
  
  eHTMLTag_tfoot,   
  eHTMLTag_th,          eHTMLTag_thead,       eHTMLTag_title,     eHTMLTag_tr,
  eHTMLTag_tt,          eHTMLTag_monofont,    eHTMLTag_u,         eHTMLTag_ul,          
  eHTMLTag_var,         eHTMLTag_wbr,         eHTMLTag_whitespace,
  eHTMLTag_xmp,         eHTMLTag_userdefined
};

//*** This enum is used to define the known universe of HTML attributes.
//*** The use of this table doesn't preclude of from using non-standard
//*** attributes. It simply makes normal tag handling more efficient.
enum eHTMLAttributes {
  eHTMLAttr_abbrev,     eHTMLAttr_above,      eHTMLAttr_alt,      eHTMLAttr_array,
  eHTMLAttr_author,     eHTMLAttr_background, eHTMLAttr_banner,   eHTMLAttr_below,
  eHTMLAttr_bgsound,    eHTMLAttr_box,        eHTMLAttr_bt,       eHTMLAttr_class,
  eHTMLAttr_comment,    eHTMLAttr_credit,     eHTMLAttr_dir,      eHTMLAttr_figure,
  eHTMLAttr_footnote,   eHTMLAttr_height,     eHTMLAttr_id,       eHTMLAttr_lang,
  eHTMLAttr_math,       eHTMLAttr_name,       eHTMLAttr_nextid,   eHTMLAttr_nobreak,
  eHTMLAttr_note,       eHTMLAttr_option,     eHTMLAttr_overlay,  eHTMLAttr_person,
  eHTMLAttr_public,     eHTMLAttr_range,      eHTMLAttr_root,     eHTMLAttr_sgml,
  eHTMLAttr_sqrt,       eHTMLAttr_src,        eHTMLAttr_style,    eHTMLAttr_text,
  eHTMLAttr_title,      eHTMLAttr_wordbreak,  eHTMLAttr_width,    eHTMLAttr_xmp
};

PRInt32         ConsumeQuotedString(PRUnichar aChar,nsString& aString,CScanner& aScanner);
PRInt32         ConsumeAttributeText(PRUnichar aChar,nsString& aString,CScanner& aScanner);
PRInt32         FindEntityIndex(const char* aBuffer,PRInt32 aBufLen=-1);
eHTMLTags       DetermineHTMLTagType(const nsString& aString);
eHTMLTokenTypes DetermineTokenType(const nsString& aString);
const char*     GetTagName(PRInt32 aTag);


/**
 *  This declares the basic token type used in the html-
 *  parser.
 *  
 *  @update  gess 3/25/98
 */
class CHTMLToken : public CToken {
public:
                        CHTMLToken(const nsString& aString);
  virtual   eHTMLTags   GetHTMLTag();
            void        SetHTMLTag(eHTMLTags aTagType);
protected:
            eHTMLTags   mTagType;
};


/**
 *  This declares start tokens, which always take the 
 *  form <xxxx>. This class also knows how to consume
 *  related attributes.
 *  
 *  @update  gess 3/25/98
 */
class CStartToken: public CHTMLToken {
  public:
                        CStartToken(const nsString& aString);
    virtual PRInt32     Consume(PRUnichar aChar,CScanner& aScanner);
    virtual eHTMLTags   GetHTMLTag();
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
            void        SetAttributed(PRBool aValue);
            PRBool      IsAttributed(void);
    virtual void        DebugDumpSource(ostream& out);
  
  protected:
            PRBool      mAttributed;      

};


/**
 *  This declares end tokens, which always take the 
 *  form </xxxx>. This class also knows how to consume
 *  related attributes.
 *  
 *  @update  gess 3/25/98
 */
class CEndToken: public CHTMLToken {
  public:
                        CEndToken(const nsString& aString);
    virtual PRInt32     Consume(PRUnichar aChar,CScanner& aScanner);
    virtual eHTMLTags   GetHTMLTag();
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
    virtual void        DebugDumpSource(ostream& out);
};


/**
 *  This declares comment tokens. Comments are usually 
 *  thought of as tokens, but we treat them that way 
 *  here so that the parser can have a consistent view
 *  of all tokens.
 *  
 *  @update  gess 3/25/98
 */
class CCommentToken: public CHTMLToken {
  public:
                        CCommentToken(const nsString& aString);
     virtual PRInt32     Consume(PRUnichar aChar,CScanner& aScanner);
    virtual const char*  GetClassName(void);
    virtual PRInt32      GetTokenType(void);
            char        mLeadingChar;
};


/**
 *  This class declares entity tokens, which always take
 *  the form &xxxx;. This class also offers a few utility
 *  methods that allow you to easily reduce entities.
 *  
 *  @update  gess 3/25/98
 */
class CEntityToken : public CHTMLToken {
  public:
                        CEntityToken(const nsString& aString);
    virtual const char*  GetClassName(void);
    virtual PRInt32      GetTokenType(void);
            PRInt32     TranslateToUnicodeStr(nsString& aString);
     virtual PRInt32     Consume(PRUnichar aChar,CScanner& aScanner);
    static  PRInt32     ConsumeEntity(PRUnichar aChar,nsString& aString,CScanner& aScanner);
    static  PRInt32     TranslateToUnicodeStr(PRInt32 aValue,nsString& aString);
    static  PRInt32     FindEntityIndex(const char* aBuffer,PRInt32 aBufLen=-1);
    static  PRBool      VerifyEntityTable(void);
    static  PRInt32     ReduceEntities(nsString& aString);
    virtual  void        DebugDumpSource(ostream& out);

  private:
    static  PRInt32     mEntityTokenCount;
};


/**
 *  Whitespace tokens are used where whitespace can be 
 *  detected as distinct from text. This allows us to 
 *  easily skip leading/trailing whitespace when desired.
 *  
 *  @update  gess 3/25/98
 */
class CWhitespaceToken: public CHTMLToken {
  public:
                        CWhitespaceToken(const nsString& aString);
     virtual PRInt32    Consume(PRUnichar aChar,CScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
};

/**
 *  Text tokens contain the normalized form of html text.
 *  These tokens are guaranteed not to contain entities,
 *  start or end tags, or newlines.
 *  
 *  @update  gess 3/25/98
 */
class CTextToken: public CHTMLToken {
  public:
                        CTextToken(const nsString& aString);
    virtual PRInt32     Consume(PRUnichar aChar,CScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
};


/**
 *  Attribute tokens are used to contain attribute key/value
 *  pairs whereever they may occur. Typically, they should
 *  occur only in start tokens. However, we may expand that
 *  ability when XML tokens become commonplace.
 *  
 *  @update  gess 3/25/98
 */
class CAttributeToken: public CHTMLToken {
  public:
                          CAttributeToken(const nsString& aString);
     virtual PRInt32       Consume(PRUnichar aChar,CScanner& aScanner);
    virtual const char*    GetClassName(void);
    virtual PRInt32        GetTokenType(void);
    virtual nsString&     GetKey(void) {return mTextKey;}
    virtual void          DebugDumpToken(ostream& out);
    virtual void          DebugDumpSource(ostream& out);
            PRBool        mLastAttribute;

  protected:
             nsString mTextKey;
};


/**
 *  Newline tokens contain, you guessed it, newlines. 
 *  They consume newline (CR/LF) either alone or in pairs.
 *  
 *  @update  gess 3/25/98
 */
class CNewlineToken: public CHTMLToken { 
  public:
                        CNewlineToken(const nsString& aString);
    virtual PRInt32     Consume(PRUnichar aChar,CScanner& aScanner);
    virtual const char* GetClassName(void);
    virtual PRInt32     GetTokenType(void);
    virtual nsString&   GetText(void);
};


/**
 *  Script tokens contain sequences of javascript (or, gulp,
 *  any other script you care to send). We don't tokenize
 *  it here, nor validate it. We just wrap it up, and pass
 *  it along to the html parser, who sends it (later on) 
 *  to the scripting engine.
 *  
 *  @update  gess 3/25/98
 */
class CScriptToken: public CHTMLToken {
  public:

                        CScriptToken(const nsString& aString);
    virtual const char*  GetClassName(void);
    virtual PRInt32      GetTokenType(void);
  protected:
};


/**
 *  Style tokens contain sequences of css style. We don't 
 *  tokenize it here, nor validate it. We just wrap it up, 
 *  and pass it along to the html parser, who sends it 
 *  (later on) to the style engine.
 *  
 *  @update  gess 3/25/98
 */
class CStyleToken: public CHTMLToken {
  public:
                        CStyleToken(const nsString& aString);
    virtual const char*  GetClassName(void);
    virtual PRInt32      GetTokenType(void);
  protected:
};


/**
 *  This is a placeholder token, which is being deprecated.
 *  Don't bother paying attention to this.
 *  
 *  @update  gess 3/25/98
 */
class CSkippedContentToken: public CAttributeToken {
  public:
                        CSkippedContentToken(const nsString& aString);
     virtual PRInt32     Consume(PRUnichar aChar,CScanner& aScanner);
    virtual const char*  GetClassName(void);
    virtual PRInt32      GetTokenType(void);
  protected:
};


#endif




