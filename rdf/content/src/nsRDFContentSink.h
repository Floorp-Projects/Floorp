/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsRDFContentSink_h__
#define nsRDFContentSink_h__

#include "nsIRDFContentSink.h"

class nsIURL;
class nsVoidArray;
class nsIRDFResource;
class nsIRDFDataSource;
class nsIRDFService;
class nsINameSpaceManager;

typedef enum {
    eRDFContentSinkState_InProlog,
    eRDFContentSinkState_InDocumentElement,
    eRDFContentSinkState_InDescriptionElement,
    eRDFContentSinkState_InContainerElement,
    eRDFContentSinkState_InPropertyElement,
    eRDFContentSinkState_InMemberElement,
    eRDFContentSinkState_InEpilog
} RDFContentSinkState;


class nsRDFContentSink : public nsIRDFContentSink
{
public:
    nsRDFContentSink();
    virtual ~nsRDFContentSink();

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIContentSink
    NS_IMETHOD WillBuildModel(void);
    NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
    NS_IMETHOD WillInterrupt(void);
    NS_IMETHOD WillResume(void);
    NS_IMETHOD SetParser(nsIParser* aParser);  
    NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
    NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
    NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
    NS_IMETHOD AddComment(const nsIParserNode& aNode);
    NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
    NS_IMETHOD NotifyError(nsresult aErrorResult);

    // nsIXMLContentSink
    NS_IMETHOD AddXMLDecl(const nsIParserNode& aNode);
    NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode);
    NS_IMETHOD AddCharacterData(const nsIParserNode& aNode);
    NS_IMETHOD AddUnparsedEntity(const nsIParserNode& aNode);
    NS_IMETHOD AddNotation(const nsIParserNode& aNode);
    NS_IMETHOD AddEntityReference(const nsIParserNode& aNode);

    // nsIRDFContentSink
    NS_IMETHOD SetDataSource(nsIRDFDataSource* ds);
    NS_IMETHOD GetDataSource(nsIRDFDataSource*& ds);
    NS_IMETHOD Init(nsIURL* aURL, nsINameSpaceManager* aNameSpaceManager);

protected:
    // Text management
    nsresult FlushText(PRBool aCreateTextNode=PR_TRUE,
                       PRBool* aDidFlush=nsnull);

    PRUnichar* mText;
    PRInt32 mTextLength;
    PRInt32 mTextSize;
    PRBool mConstrainSize;

    // namespace management
    void      PushNameSpacesFrom(const nsIParserNode& aNode);
    nsIAtom*  CutNameSpacePrefix(nsString& aString);
    PRInt32   GetNameSpaceID(nsIAtom* aPrefix);
    void      GetNameSpaceURI(PRInt32 aID, nsString& aURI);
    void      PopNameSpaces();

    nsINameSpaceManager*  mNameSpaceManager;
    nsVoidArray* mNameSpaceStack;
    PRInt32      mRDFNameSpaceID;

    void SplitQualifiedName(const nsString& aQualifiedName,
                            PRInt32& rNameSpaceID,
                            nsString& rProperty);

    // RDF-specific parsing
    nsresult GetIdAboutAttribute(const nsIParserNode& aNode, nsString& rResource);
    nsresult GetResourceAttribute(const nsIParserNode& aNode, nsString& rResource);
    nsresult AddProperties(const nsIParserNode& aNode, nsIRDFResource* aSubject);

    virtual nsresult OpenRDF(const nsIParserNode& aNode);
    virtual nsresult OpenObject(const nsIParserNode& aNode);
    virtual nsresult OpenProperty(const nsIParserNode& aNode);
    virtual nsresult OpenMember(const nsIParserNode& aNode);
    virtual nsresult OpenValue(const nsIParserNode& aNode);

    // Miscellaneous RDF junk
    nsIRDFService*         mRDFService;
    nsIRDFDataSource*      mDataSource;
    RDFContentSinkState    mState;

    // content stack management
    PRInt32         PushContext(nsIRDFResource *aContext, RDFContentSinkState aState);
    nsresult        PopContext(nsIRDFResource*& rContext, RDFContentSinkState& rState);
    nsIRDFResource* GetContextElement(PRInt32 ancestor = 0);

    nsVoidArray* mContextStack;

    nsIURL*      mDocumentURL;
    PRUint32     mGenSym; // for generating anonymous resources
};


#endif // nsRDFContentSink_h__
