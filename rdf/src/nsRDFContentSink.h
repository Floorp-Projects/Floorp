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
#include "nsIViewManager.h"
#include "nsIScrollableView.h"

class nsIDocument;
class nsIScriptObjectOwner;
class nsIURL;
class nsIWebShell;
class nsIContent;
class nsVoidArray;
class nsIRDFDocument;
class nsIRDFContent;
class nsIRDFNode;
class nsIRDFResourceManager;
class nsIUnicharInputStream;
class nsIStyleSheet;

typedef enum {
    eRDFContentSinkState_InProlog,
    eRDFContentSinkState_InDocumentElement,
    eRDFContentSinkState_InDescriptionElement,
    eRDFContentSinkState_InContainerElement,
    eRDFContentSinkState_InPropertyElement,
    eRDFContentSinkState_InMemberElement,
    eRDFContentSinkState_InEpilog
} RDFContentSinkState;


class nsRDFContentSink : public nsIRDFContentSink {
public:
    nsRDFContentSink();
    ~nsRDFContentSink();

    nsresult Init(nsIDocument* aDoc,
                  nsIURL* aURL,
                  nsIWebShell* aContainer);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIContentSink
    NS_IMETHOD WillBuildModel(void);
    NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
    NS_IMETHOD WillInterrupt(void);
    NS_IMETHOD WillResume(void);
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

protected:
    void StartLayout();

    nsresult LoadStyleSheet(nsIURL* aURL,
                            nsIUnicharInputStream* aUIN);
    nsresult FlushText(PRBool aCreateTextNode=PR_TRUE,
                       PRBool* aDidFlush=nsnull);

    void FindNameSpaceAttributes(const nsIParserNode& aNode);

    // namespace management
    PRInt32 OpenNameSpace(const nsString& aPrefix, const nsString& aURI);
    PRInt32 GetNameSpaceId(const nsString& aPrefix);
    void    CloseNameSpacesAtNestLevel(PRInt32 mNestLevel);
  
    nsresult SplitQualifiedName(const nsString& aQualifiedName,
                                nsString& rNameSpaceURI,
                                nsString& rPropertyURI);

    // RDF-specific parsing
    nsresult GetIdAboutAttribute(const nsIParserNode& aNode, nsString& rResource);
    nsresult GetResourceAttribute(const nsIParserNode& aNode, nsString& rResource);
    nsresult AddProperties(const nsIParserNode& aNode, nsIRDFNode* aSubject);

    nsresult OpenRDF(const nsIParserNode& aNode);
    nsresult OpenObject(const nsIParserNode& aNode);
    nsresult OpenProperty(const nsIParserNode& aNode);
    nsresult OpenMember(const nsIParserNode& aNode);
    nsresult OpenValue(const nsIParserNode& aNode);

    // RDF helper routines
    nsresult Assert(nsIRDFNode* subject, nsIRDFNode* predicate, nsIRDFNode* object);
    nsresult Assert(nsIRDFNode* subject, nsIRDFNode* predicate, const nsString& objectLiteral);
    nsresult Assert(nsIRDFNode* subject, const nsString& predicateURI, const nsString& objectLiteral);

    // content stack management
    PRInt32     PushContext(nsIRDFNode *aContext, RDFContentSinkState aState);
    nsresult    PopContext(nsIRDFNode*& rContext, RDFContentSinkState& rState);
    nsIRDFNode* GetContextElement(PRInt32 ancestor = 0);
    PRInt32     GetCurrentNestLevel();

    struct NameSpaceStruct {
        nsIAtom* mPrefix;
        PRInt32 mId;
        PRInt32 mNestLevel;
    };

    nsIDocument* mDocument;
    nsIURL*      mDocumentURL;
    nsIWebShell* mWebShell;
    nsIContent*  mRootElement;
    PRUint32     mGenSym; // for generating anonymous resources

    nsIRDFResourceManager* mRDFResourceManager;
    nsIRDFDataSource* mDataSource; // XXX should this really be a rdf *db* vs. a raw datasource?
    RDFContentSinkState mState;
    nsVoidArray* mNameSpaces;

    PRInt32 mNestLevel;
    nsVoidArray* mContextStack;

    nsIStyleSheet* mStyleSheet;
    nsScrollPreference mOriginalScrollPreference;

    PRUnichar* mText;
    PRInt32 mTextLength;
    PRInt32 mTextSize;
    PRBool mConstrainSize;
};

#endif // nsRDFContentSink_h__
