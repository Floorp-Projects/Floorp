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
#include "nsIXULSortService.h"

class nsIRDFDocument;
class nsIRDFCompositeDataSource;
class nsIRDFResource;
class nsIContent;
class nsIRDFNode;
class nsIAtom;
class nsIRDFService;
class nsIHTMLElementFactory;

class RDFGenericBuilderImpl : public nsIRDFContentModelBuilder,
                              public nsIRDFObserver,
                              public nsIDOMNodeObserver,
                              public nsIDOMElementObserver
{
public:
    RDFGenericBuilderImpl();
    virtual ~RDFGenericBuilderImpl();

    nsresult Init();

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
    FindTemplate(nsIContent* aElement,
                 nsIRDFResource* aProperty,
                 nsIRDFResource* aChild,
                 nsIContent **theTemplate);

    nsresult
    IsTemplateRuleMatch(nsIContent* aElement,
                        nsIRDFResource* aProperty,
                        nsIRDFResource* aChild,
                        nsIContent *aRule,
                        PRBool *isMatch);

    PRBool
    IsIgnoreableAttribute(PRInt32 aNameSpaceID, nsIAtom* aAtom);

    nsresult
    GetSubstitutionText(nsIRDFResource* aResource,
                        const nsString& aSubstitution,
                        nsString& aResult);

    nsresult
    BuildContentFromTemplate(nsIContent *aTemplateNode,
                             nsIContent *aRealNode,
                             PRBool aIsUnique,
                             nsIRDFResource* aChild,
                             PRInt32 aNaturalOrderPos,
                             PRBool aNotify);

    nsresult
    CreateWidgetItem(nsIContent* aElement,
                     nsIRDFResource* aProperty,
                     nsIRDFResource* aChild,
                     PRInt32 aNaturalOrderPos,
                     PRBool aNotify);

    enum eUpdateAction { eSet, eClear };

    PRBool
    IsAttributePersisent(nsIContent *element, PRInt32 aNameSpaceID, nsIAtom* aAtom);

    void
    GetPersistentAttributes(nsIContent *realKid);

    void
    PersistAttribute(nsIContent *element,
    			PRInt32 aNameSpaceID,
    			nsIAtom* aAtom,
    			nsString aValue,
    			eUpdateAction action);

    void
    PersistProperty(nsIContent *element,
    			nsIRDFResource *aProperty,
    			nsIRDFNode *aTarget,
    			eUpdateAction action);

    nsresult
    SynchronizeUsingTemplate(nsIContent *aTemplateNode,
                             nsIContent* aRealNode,
                             eUpdateAction aAction,
                             nsIRDFResource* aProperty,
                             nsIRDFNode* aValue);

    nsresult
    RemoveWidgetItem(nsIContent* aElement,
                     nsIRDFResource* aProperty,
                     nsIRDFResource* aValue,
                     PRBool aNotify);

    nsresult
    CreateContainerContents(nsIContent* aElement, nsIRDFResource* aResource);

    nsresult
    CreateTemplateContents(nsIContent* aElement, const nsString& aTemplateID);

    nsresult
    EnsureElementHasGenericChild(nsIContent* aParent,
                                 PRInt32 aNameSpaceID,
                                 nsIAtom* aTag,
                                 nsIContent** aResult);

    PRBool
    IsContainmentProperty(nsIContent* aElement, nsIRDFResource* aProperty);

    PRBool
    IsIgnoredProperty(nsIContent* aElement, nsIRDFResource* aProperty);

    PRBool
    IsContainer(nsIContent* aParentElement, nsIRDFResource* aTargetResource);

    PRBool
    IsEmpty(nsIContent* aParentElement, nsIRDFResource* aContainer);

    PRBool
    IsOpen(nsIContent* aElement);

    PRBool
    IsElementInWidget(nsIContent* aElement);
   
    PRBool
    IsResourceElement(nsIContent* aElement);
 
    nsresult
    GetDOMNodeResource(nsIDOMNode* aNode, nsIRDFResource** aResource);

    nsresult
    GetResource(PRInt32 aNameSpaceID,
                nsIAtom* aNameAtom,
                nsIRDFResource** aResource);

    nsresult
    OpenWidgetItem(nsIContent* aElement);

    nsresult
    CloseWidgetItem(nsIContent* aElement);

    nsresult
    RemoveAndRebuildGeneratedChildren(nsIContent* aElement);

    // XXX. Urg. Hack until layout can batch reflows. See bug 10818.
    PRBool
    IsTreeWidgetItem(nsIContent* aElement);

    PRBool
    IsReflowScheduled();

    nsresult
    ScheduleReflow();

    static void
    ForceTreeReflow(nsITimer* aTimer, void* aClosure);

protected:
    nsIRDFDocument*            mDocument; // [WEAK]

    // We are an observer of the composite datasource. The cycle is
    // broken by out-of-band SetDataBase(nsnull) call when document is
    // destroyed.
    nsCOMPtr<nsIRDFCompositeDataSource> mDB;
    nsCOMPtr<nsIContent>                mRoot;

    nsCOMPtr<nsITimer> mTimer;

    static nsIRDFDataSource	*mLocalstore;
    static PRBool		persistLock;

    // pseudo-constants
    static nsrefcnt gRefCnt;
    static nsIRDFService*         gRDFService;
    static nsIRDFContainerUtils*  gRDFContainerUtils;
    static nsINameSpaceManager*   gNameSpaceManager;
    static nsIHTMLElementFactory* gHTMLElementFactory;

    static nsIAtom* kContainerAtom;
    static nsIAtom* kLazyContentAtom;
    static nsIAtom* kIsContainerAtom;
    static nsIAtom* kIsEmptyAtom;
    static nsIAtom* kXULContentsGeneratedAtom;
    static nsIAtom* kTemplateContentsGeneratedAtom;
    static nsIAtom* kContainerContentsGeneratedAtom;
    static nsIAtom* kNaturalOrderPosAtom;
    static nsIAtom* kIdAtom;
    static nsIAtom* kPersistAtom;
    static nsIAtom* kOpenAtom;
    static nsIAtom* kEmptyAtom;
    static nsIAtom* kResourceAtom;
    static nsIAtom* kURIAtom;
    static nsIAtom* kContainmentAtom;
    static nsIAtom* kIgnoreAtom;
    static nsIAtom* kRefAtom;
    static nsIAtom* kValueAtom;

    static nsIAtom* kTemplateAtom;
    static nsIAtom* kRuleAtom;
    static nsIAtom* kTextAtom;
    static nsIAtom* kPropertyAtom;
    static nsIAtom* kInstanceOfAtom;

    static nsIAtom* kTreeAtom;
    static nsIAtom* kTreeChildrenAtom;
    static nsIAtom* kTreeItemAtom;

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
