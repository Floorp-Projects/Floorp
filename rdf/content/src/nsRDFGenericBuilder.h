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

/*

  An implementation that builds a XUL
  content model that is to be used with a certain widget.  This class
  is abstract.
 */

#include "nsIRDFContainerUtils.h" 
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFObserver.h"
#include "nsIDOMNodeObserver.h"
#include "nsIDOMElementObserver.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "nsIXULSortService.h"

class nsIRDFDocument;
class nsIRDFCompositeDataSource;
class nsIRDFResource;
class nsIContent;
class nsIRDFNode;
class nsIAtom;
class nsIRDFService;

class RDFGenericBuilderImpl : public nsIRDFContentModelBuilder,
                              public nsIRDFObserver,
                              public nsIDOMNodeObserver,
                              public nsIDOMElementObserver,
                              public nsITimerCallback
{
public:
    RDFGenericBuilderImpl();
    virtual ~RDFGenericBuilderImpl();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFContentModelBuilder interface
    NS_IMETHOD SetDocument(nsIRDFDocument* aDocument);
    NS_IMETHOD SetDataBase(nsIRDFCompositeDataSource* aDataBase);
    NS_IMETHOD GetDataBase(nsIRDFCompositeDataSource** aDataBase);
    NS_IMETHOD CreateRootContent(nsIRDFResource* aResource);
    NS_IMETHOD SetRootContent(nsIContent* aElement);
    NS_IMETHOD CreateContents(nsIContent* aElement);
    NS_IMETHOD CreateElement(PRInt32 aNameSpaceID,
                             nsIAtom* aTag,
                             nsIRDFResource* aResource,
                             nsIContent** aResult);

    // nsIRDFObserver interface
    NS_IMETHOD OnAssert(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aTarget);

    NS_IMETHOD OnUnassert(nsIRDFResource* aSource,
                          nsIRDFResource* aProperty,
                          nsIRDFNode* aTarget);

    NS_IMETHOD OnChange(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aOldTarget,
                        nsIRDFNode* aNewTarget);

    NS_IMETHOD OnMove(nsIRDFResource* aOldSource,
                      nsIRDFResource* aNewSource,
                      nsIRDFResource* aProperty,
                      nsIRDFNode* aTarget);

    // nsIDOMNodeObserver interface
    NS_DECL_IDOMNODEOBSERVER

    // nsIDOMElementObserver interface
    NS_DECL_IDOMELEMENTOBSERVER

    // Implementation methods
    nsresult
    SetAllAttributesOnElement(nsIContent *aParentNode, nsIContent *aNode, nsIRDFResource *res);
    
    nsresult
    FindTemplateForResource(nsIRDFResource *aNode, nsIContent **theTemplate);

    nsresult
    IsTemplateRuleMatch(nsIRDFResource *aNode, nsIContent *aRule, PRBool *matchingRuleFound);

    PRBool
    IsIgnoreableAttribute(PRInt32 aNameSpaceID, nsIAtom* aAtom);

    nsresult
    BuildContentFromTemplate(nsIContent *aTemplateNode,
                             nsIContent *aRealNode,
                             PRBool aIsUnique,
                             nsIRDFResource* aValue,
                             PRInt32 aNaturalOrderPos);

    nsresult
    CreateWidgetItem(nsIContent* aElement,
                     nsIRDFResource* aProperty,
                     nsIRDFResource* aValue,
                     PRInt32 aNaturalOrderPos);

    enum eUpdateAction { eSet, eClear };

    nsresult
    SynchronizeUsingTemplate(nsIContent *aTemplateNode,
                             nsIContent* aRealNode,
                             eUpdateAction aAction,
                             nsIRDFResource* aProperty,
                             nsIRDFNode* aValue);

    nsresult
    CreateContainerContents(nsIContent* aElement, nsIRDFResource* aResource);

    nsresult
    CreateTemplateContents(nsIContent* aElement, const nsString& aTemplateID);

    nsresult
    EnsureElementHasGenericChild(nsIContent* aParent,
                                 PRInt32 aNameSpaceID,
                                 nsIAtom* aTag,
                                 nsIContent** aResult);

    virtual nsresult
    AddWidgetItem(nsIContent* aWidgetElement,
                  nsIRDFResource* aProperty,
                  nsIRDFResource* aValue, 
                  PRInt32 naturalOrderPos) = 0;

    virtual nsresult
    RemoveWidgetItem(nsIContent* aWidgetElement,
                     nsIRDFResource* aProperty,
                     nsIRDFResource* aValue) = 0;

    virtual nsresult
    SetWidgetAttribute(nsIContent* aWidgetElement,
                       nsIRDFResource* aProperty,
                       nsIRDFNode* aValue) = 0;

    virtual nsresult
    UnsetWidgetAttribute(nsIContent* aWidgetElement,
                         nsIRDFResource* aProperty,
                         nsIRDFNode* aValue) = 0;

    virtual PRBool
    IsContainmentProperty(nsIContent* aElement, nsIRDFResource* aProperty);

    virtual PRBool
    IsIgnoredProperty(nsIContent* aElement, nsIRDFResource* aProperty);

    PRBool
    IsContainer(nsIContent* aParentElement, nsIRDFResource* aTargetResource, PRBool trackEmptyFlag = PR_TRUE);

    PRBool
    IsOpen(nsIContent* aElement);

    PRBool
    IsElementInWidget(nsIContent* aElement);
   
    PRBool
    IsItemOrFolder(nsIContent* aElement);
 
    PRBool
    IsWidgetInsertionRootElement(nsIContent* aElement);

    nsresult
    GetDOMNodeResource(nsIDOMNode* aNode, nsIRDFResource** aResource);

    nsresult
    GetResource(PRInt32 aNameSpaceID,
                nsIAtom* aNameAtom,
                nsIRDFResource** aResource);

    virtual nsresult
    OpenWidgetItem(nsIContent* aElement);

    virtual nsresult
    CloseWidgetItem(nsIContent* aElement);

    virtual nsresult
    GetRootWidgetAtom(nsIAtom** aResult) = 0;

    virtual nsresult
    GetWidgetItemAtom(nsIAtom** aResult) = 0;

    virtual nsresult
    GetWidgetFolderAtom(nsIAtom** aResult) = 0;

    virtual nsresult
    GetInsertionRootAtom(nsIAtom** aResult) = 0;

    virtual nsresult
    GetItemAtomThatContainsTheChildren(nsIAtom** aResult) = 0;
    // Well, you come up with a better name.

protected:
    nsIRDFDocument*            mDocument;
    nsIRDFCompositeDataSource* mDB;
    nsIContent*                mRoot;

    nsITimer			*mTimer;

    virtual void
    Notify(nsITimer *timer) = 0;

    // pseudo-constants
    static nsrefcnt gRefCnt;
    static nsIRDFService*        gRDFService;
    static nsIRDFContainerUtils* gRDFContainerUtils;
    static nsINameSpaceManager* gNameSpaceManager;

    static nsIAtom* kContainerAtom;
    static nsIAtom* kLazyContentAtom;
    static nsIAtom* kIsContainerAtom;
    static nsIAtom* kXULContentsGeneratedAtom;
    static nsIAtom* kTemplateContentsGeneratedAtom;
    static nsIAtom* kContainerContentsGeneratedAtom;
    static nsIAtom* kNaturalOrderPosAtom;
    static nsIAtom* kIdAtom;
    static nsIAtom* kOpenAtom;
    static nsIAtom* kEmptyAtom;
    static nsIAtom* kResourceAtom;
    static nsIAtom* kURIAtom;
    static nsIAtom* kContainmentAtom;
    static nsIAtom* kIgnoreAtom;
    static nsIAtom* kRefAtom;

    static nsIAtom* kSubcontainmentAtom;
    static nsIAtom* kRootcontainmentAtom;
    static nsIAtom* kTemplateAtom;
    static nsIAtom* kRuleAtom;
    static nsIAtom* kTextAtom;
    static nsIAtom* kPropertyAtom;
    static nsIAtom* kInstanceOfAtom;

    static PRInt32  kNameSpaceID_RDF;
    static PRInt32  kNameSpaceID_XUL;

    static nsIRDFResource* kNC_Title;
    static nsIRDFResource* kNC_child;
    static nsIRDFResource* kNC_Column;
    static nsIRDFResource* kNC_Folder;
    static nsIRDFResource* kRDF_child;
    static nsIRDFResource* kRDF_instanceOf;
    static nsIRDFResource* kXUL_element;

    static nsIXULSortService	*XULSortService;
};
