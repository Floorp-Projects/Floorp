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

#include "nsHTMLTokens.h"
#include "nsXPFCXMLDTD.h"
#include "nsxpfcCIID.h"
#include "nsIXMLParserObject.h"
#include "nsXPFCXMLContentSink.h"
#include "nsIXPFCMenuBar.h"
#include "nsIXPFCMenuItem.h"
#include "nsIXPFCMenuContainer.h"
#include "nsIXPFCCanvas.h"
#include "nsIButton.h"
#include "nsITextWidget.h"
#include "nsITabWidget.h"
#include "nsWidgetsCID.h"
#include "nsXPFCToolkit.h"

static NS_DEFINE_IID(kISupportsIID,         NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIContentSinkIID,      NS_ICONTENT_SINK_IID);
static NS_DEFINE_IID(kClassIID,             NS_XPFCXMLCONTENTSINK_IID); 
static NS_DEFINE_IID(kIHTMLContentSinkIID,  NS_IHTML_CONTENT_SINK_IID);
static NS_DEFINE_IID(kIXMLParserObjectIID,  NS_IXML_PARSER_OBJECT_IID);
static NS_DEFINE_IID(kCXPFCMenuBarCID,          NS_XPFCMENUBAR_CID);
static NS_DEFINE_IID(kCXPFCMenuItemCID,         NS_XPFCMENUITEM_CID);
static NS_DEFINE_IID(kCXPFCMenuContainerCID,    NS_XPFCMENUCONTAINER_CID);
static NS_DEFINE_IID(kCIXPFCMenuBarIID,         NS_IXPFCMENUBAR_IID);
static NS_DEFINE_IID(kCIXPFCMenuItemIID,        NS_IXPFCMENUITEM_IID);
static NS_DEFINE_IID(kCIXPFCMenuContainerIID,   NS_IXPFCMENUCONTAINER_IID);
static NS_DEFINE_IID(kCXPFCToolbarCID,      NS_XPFC_TOOLBAR_CID);
static NS_DEFINE_IID(kCIXPFCToolbarIID,     NS_IXPFC_TOOLBAR_IID);
static NS_DEFINE_IID(kCXPFCDialogCID,       NS_XPFC_DIALOG_CID);
static NS_DEFINE_IID(kCIXPFCDialogIID,      NS_IXPFC_DIALOG_IID);
static NS_DEFINE_IID(kCXPFCCanvasCID,       NS_XPFC_CANVAS_CID);
static NS_DEFINE_IID(kIXPFCCanvasIID,       NS_IXPFC_CANVAS_IID);
static NS_DEFINE_IID(kCButtonCID,           NS_BUTTON_CID);
static NS_DEFINE_IID(kCTextWidgetCID,       NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kCTabWidgetCID,        NS_TABWIDGET_CID);
static NS_DEFINE_IID(kCXPFCButtonCID,       NS_XPFC_BUTTON_CID);
static NS_DEFINE_IID(kCXPButtonCID,         NS_XP_BUTTON_CID);
static NS_DEFINE_IID(kCXPFCTabWidgetCID,    NS_XPFC_TABWIDGET_CID);
static NS_DEFINE_IID(kCXPFCTextWidgetCID,   NS_XPFC_TEXTWIDGET_CID);
static NS_DEFINE_IID(kIXPFCXMLContentSinkIID,  NS_IXPFC_XML_CONTENT_SINK_IID); 

#define XPFC_PARSING_STATE_UNKNOWN      0
#define XPFC_PARSING_STATE_TOOLBAR      1
#define XPFC_PARSING_STATE_MENUBAR      2
#define XPFC_PARSING_STATE_DIALOG       3
#define XPFC_PARSING_STATE_APPLICATION  4

class ContainerListEntry {
public:
  nsIXMLParserObject * object;
  nsString container;

  ContainerListEntry(nsIXMLParserObject * aObject, 
                     nsString  aContainer) { 
    object = aObject;
    container = aContainer;
  }
  ~ContainerListEntry() {
  }
};


nsXPFCXMLContentSink::nsXPFCXMLContentSink() : nsIHTMLContentSink()
{
  NS_INIT_REFCNT();
  mViewerContainer = nsnull;
  mContainerList = nsnull;

  mState = XPFC_PARSING_STATE_UNKNOWN;

  static NS_DEFINE_IID(kCVectorCID, NS_VECTOR_CID);
  nsresult res = nsRepository::CreateInstance(kCVectorCID, 
                                              nsnull, 
                                              kCVectorCID, 
                                              (void **)&mOrphanMenuList);

  if (NS_OK != res)
    return ;

  mOrphanMenuList->Init();

  static NS_DEFINE_IID(kCStackCID, NS_STACK_CID);

  res = nsRepository::CreateInstance(kCStackCID, 
                                     nsnull, 
                                     kCStackCID, 
                                     (void **)&mXPFCStack);

  if (NS_OK != res)
    return ;

  mXPFCStack->Init();

  if (mContainerList == nsnull) {

    static NS_DEFINE_IID(kCVectorIteratorCID, NS_VECTOR_ITERATOR_CID);
    static NS_DEFINE_IID(kCVectorCID, NS_VECTOR_CID);

    nsresult res = nsRepository::CreateInstance(kCVectorCID, 
                                       nsnull, 
                                       kCVectorCID, 
                                       (void **)&mContainerList);

    if (NS_OK != res)
      return ;

    mContainerList->Init();
  }



}

nsXPFCXMLContentSink::~nsXPFCXMLContentSink() 
{
  NS_RELEASE(mOrphanMenuList);
  NS_RELEASE(mXPFCStack);

  if (mContainerList != nsnull) {
    mContainerList->RemoveAll();
    NS_RELEASE(mContainerList);
  }
}

NS_IMPL_ADDREF(nsXPFCXMLContentSink)
NS_IMPL_RELEASE(nsXPFCXMLContentSink)


nsresult nsXPFCXMLContentSink::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIContentSink*)(this);                                        
  }
  else if(aIID.Equals(kIContentSinkIID)) {  //do nsIContentSink base class...
    *aInstancePtr = (nsIContentSink*)(this);                                        
  }
  else if(aIID.Equals(kIHTMLContentSinkIID)) {
    *aInstancePtr = (nsIHTMLContentSink*)(this);
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsXPFCXMLContentSink*)(this);                                        
  }                 
  else if(aIID.Equals(kIXPFCXMLContentSinkIID)) {  //do this class...
    *aInstancePtr = (nsIXPFCXMLContentSink*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}

NS_IMETHODIMP nsXPFCXMLContentSink::Init()
{
  return NS_OK;
}


/*
 * this method gets invoked whenever a container type tag
 * is encountered.
 */

NS_IMETHODIMP nsXPFCXMLContentSink::OpenContainer(const nsIParserNode& aNode)
{  
  nsIXMLParserObject * object = nsnull;
  eXPFCXMLTags tag = (eXPFCXMLTags) aNode.GetNodeType();
  nsresult res;
  nsCID aclass;
  nsString toLoadUrl;

  if (eXPFCXMLTag_menubar == tag)
    mState = XPFC_PARSING_STATE_MENUBAR;
  else if (eXPFCXMLTag_toolbar == tag)
    mState = XPFC_PARSING_STATE_TOOLBAR;
  else if (eXPFCXMLTag_dialog == tag)
    mState = XPFC_PARSING_STATE_DIALOG;
  else if (eXPFCXMLTag_application == tag)
    mState = XPFC_PARSING_STATE_APPLICATION;

  if (mState == XPFC_PARSING_STATE_APPLICATION)
  {
    if (eXPFCXMLTag_canvas == tag)
    {
      PRInt32 i = 0;
      nsString key,value;

      for (i = 0; i < aNode.GetAttributeCount(); i++) 
      {

       key   = aNode.GetKeyAt(i);
       value = aNode.GetValueAt(i);

       key.StripChars("\"");
       value.StripChars("\"");

       if (key.EqualsIgnoreCase("src"))
       {
        // Load the url!
        toLoadUrl = value;        
        break;
       }

      }

    }
    //return NS_OK;    
  }

  nsString text = aNode.GetText();

  res = CIDFromTag(tag, aclass);

  if (NS_OK != res)
    return res ;

  res = nsRepository::CreateInstance(aclass, 
                                     nsnull, 
                                     kIXMLParserObjectIID, 
                                     (void **)&object);

  if (NS_OK != res)
    return res ;


  /*
   * ConsumeAttributes
   */

  AddToHierarchy(*object, PR_TRUE);
  object->Init();        
  ConsumeAttributes(aNode,*object);

  /*
   * If this is a menu bar, tell the widget.
   * Probably want a general notification scheme for menubar
   * changes
   */

  if (eXPFCXMLTag_menubar == tag)
  {
    nsIXPFCMenuBar * menubar;
    res = object->QueryInterface(kCIXPFCMenuBarIID,(void**)&menubar);

    if (NS_OK == res)
    {
      mViewerContainer->SetMenuBar(menubar);    
      NS_RELEASE(menubar);
    }
  }
  /*
   * If this is a menu bar, tell the widget.
   * Probably want a general notification scheme for menubar
   * changes
   */

  if (eXPFCXMLTag_toolbar == tag)
  {

    nsIXPFCToolbar * toolbar;
    res = object->QueryInterface(kCIXPFCToolbarIID,(void**)&toolbar);

    if (NS_OK == res)
    {
      mViewerContainer->AddToolbar(toolbar);    
      NS_RELEASE(toolbar);
    }

  }

  if (eXPFCXMLTag_dialog == tag)
  {

    nsIXPFCDialog * dialog;
    res = object->QueryInterface(kCIXPFCDialogIID,(void**)&dialog);

    if (NS_OK == res)
    {
      mViewerContainer->ShowDialog(dialog);    
      NS_RELEASE(dialog);
    }

  }

  /*
   * If this was a root panel, add it to the widget
   */

  if (eXPFCXMLTag_application == tag)
  {
    nsIXPFCCanvas * child = (nsIXPFCCanvas *) mXPFCStack->Top();

    if (child != nsnull)
    {
      nsIXPFCCanvas * root ;

      gXPFCToolkit->GetRootCanvas(&root);

      root->DeleteChildren();

      root->AddChildCanvas(child);

      NS_RELEASE(root);
    }
  }

  if (toLoadUrl.Length() > 0)
  {
    nsIXPFCCanvas * canvas = nsnull;

    if (toLoadUrl.Find(".cal") != -1)
      object->QueryInterface(kIXPFCCanvasIID,(void**)&canvas);

    mViewerContainer->LoadURL(toLoadUrl,nsnull,canvas);

    NS_IF_RELEASE(canvas);
  }

  // XXX: Really need this for all states
  if (mState == XPFC_PARSING_STATE_MENUBAR)
    NS_RELEASE(object);

  return NS_OK;
}


NS_IMETHODIMP nsXPFCXMLContentSink::CloseContainer(const nsIParserNode& aNode)
{  
  nsISupports * container = (nsISupports *)mXPFCStack->Pop();  

  /*
   * If we popped the topmost canvas, lay it out
   */

  nsISupports * parent = (nsISupports *) mXPFCStack->Top();
  if (parent == nsnull && container != nsnull)
  {
    nsIXPFCCanvas * canvas ;
    nsresult res = container->QueryInterface(kIXPFCCanvasIID,(void**)&canvas);

    if (res == NS_OK)
    {
      canvas->Layout();
      NS_RELEASE(canvas);
    }
  }

#if 0
  NS_IF_RELEASE(container);
#endif

  return NS_OK;
}


NS_IMETHODIMP nsXPFCXMLContentSink::AddLeaf(const nsIParserNode& aNode)
{
  nsIXMLParserObject * object = nsnull;
  eXPFCXMLTags tag = (eXPFCXMLTags) aNode.GetNodeType();
  nsresult res;
  nsCID aclass;

  // XXX Til we implement separators
  if (tag == eXPFCXMLTag_separator)
    return NS_OK;

  /*
   * The XML may specify this as a leaf, but it's parameters
   * will reference a child object, in which case it's 
   * runtime is a container
   */

  if (IsContainer(aNode) == PR_TRUE)
  {
    res = OpenContainer(aNode);
    CloseContainer(aNode);
    return res;
  }


  nsString text = aNode.GetText();

  res = CIDFromTag(tag, aclass);


  res = nsRepository::CreateInstance(aclass, 
                                     nsnull, 
                                     kIXMLParserObjectIID, 
                                     (void **)&object);

  if (NS_OK != res)
    return res ;

  AddToHierarchy(*object, PR_FALSE);

  object->Init();        

  ConsumeAttributes(aNode,*object);

  // XXX: Need a way to identify that native widgets are
  //      involved!
  if (tag == eXPFCXMLTag_button 
   || tag == eXPFCXMLTag_tabwidget
   || tag == eXPFCXMLTag_editfield)
  {
    nsIXPFCCanvas * canvas = nsnull;
    res = object->QueryInterface(kIXPFCCanvasIID,(void**)&canvas);
    if (NS_OK == res)
    {
      canvas->CreateView();
      NS_RELEASE(canvas);
    }

  }

  // XXX: Really need this for all states
  if (mState == XPFC_PARSING_STATE_MENUBAR)
    NS_RELEASE(object);

  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::PushMark()
{
  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::OpenHTML(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::CloseHTML(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::OpenHead(const nsIParserNode& aNode) 
{
  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::CloseHead(const nsIParserNode& aNode) 
{
  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::SetTitle(const nsString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::OpenBody(const nsIParserNode& aNode) 
{
  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::CloseBody(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::OpenForm(const nsIParserNode& aNode) 
{
  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::CloseForm(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::OpenMap(const nsIParserNode& aNode) 
{
  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::CloseMap(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::OpenFrameset(const nsIParserNode& aNode) 
{
  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::CloseFrameset(const nsIParserNode& aNode)
{
  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::WillBuildModel(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::DidBuildModel(PRInt32 aQualityLevel)
{

  if (mState == XPFC_PARSING_STATE_MENUBAR)
  {
    nsIIterator * iterator;

    mContainerList->CreateIterator(&iterator);

    iterator->Init();

    ContainerListEntry * item ;

    while(!(iterator->IsDone()))
    {
      item = (ContainerListEntry *) iterator->CurrentItem();

      /*
       * item->object is the parent container
       * item->container is the name of the target child container
       *
       * There should be a nsIXPFCMenuContainer in the free list with this
       * name.  Let's find it, make it the child, and remove from free list
       */

        nsIIterator * iterator2 ;
        nsIXPFCMenuContainer * menu_container;
        nsString child;

        nsresult res = mOrphanMenuList->CreateIterator(&iterator2);

        if (NS_OK != res)
          return nsnull;

        iterator2->Init();

        while(!(iterator2->IsDone()))
        {
          menu_container = (nsIXPFCMenuContainer *) iterator2->CurrentItem();


          nsIXPFCMenuItem * container_item = nsnull;

          res = menu_container->QueryInterface(kCIXPFCMenuItemIID,(void**)&container_item);

          if (res == NS_OK)
          {

            child = container_item->GetName();

            if (child == item->container) {

              /*
               * Set as child
               */

              nsIXPFCMenuContainer * parent = nsnull;
              res = item->object->QueryInterface(kCIXPFCMenuContainerIID, (void**)&parent);

              if (res == NS_OK)
              {
                parent->AddChild((nsIXPFCMenuItem *)container_item);
                NS_RELEASE(parent);
              }

              mOrphanMenuList->Remove(child);
              break;
            }

            NS_RELEASE(container_item);
          }

          iterator2->Next();
        }

        NS_RELEASE(iterator2);

      iterator->Next();
    }

    NS_IF_RELEASE(iterator);

    mViewerContainer->UpdateMenuBar();
  } else if (mState == XPFC_PARSING_STATE_TOOLBAR)
  {
    mViewerContainer->UpdateToolbars();
  } 

  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::WillInterrupt(void) 
{
  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::WillResume(void) 
{
  return NS_OK;
}


nsresult nsXPFCXMLContentSink::SetViewerContainer(nsIWebViewerContainer * aViewerContainer)
{
  mViewerContainer = aViewerContainer;

  return NS_OK;
}


/*
 * Find the CID from the XML Tag Object
 */

NS_IMETHODIMP nsXPFCXMLContentSink::CIDFromTag(eXPFCXMLTags tag, nsCID &aClass)
{
  switch(tag)
  {
    case eXPFCXMLTag_menuitem:
      aClass = kCXPFCMenuItemCID;
      break;

    case eXPFCXMLTag_menucontainer:
      aClass = kCXPFCMenuContainerCID;
      break;

    case eXPFCXMLTag_menubar:
      aClass = kCXPFCMenuBarCID;
      break;

    case eXPFCXMLTag_toolbar:
      aClass = kCXPFCToolbarCID;
      break;

    case eXPFCXMLTag_dialog:
      aClass = kCXPFCDialogCID;
      break;

    case eXPFCXMLTag_application:
    case eXPFCXMLTag_canvas:
    case eXPFCXMLTag_separator:
      aClass = kCXPFCCanvasCID;
      break;
      
    case eXPFCXMLTag_editfield:
      aClass = kCXPFCTextWidgetCID;
      break;
      
    case eXPFCXMLTag_button:
      aClass = kCXPFCButtonCID;
      break;

    case eXPFCXMLTag_xpbutton:
      aClass = kCXPButtonCID;
      break;
      
    case eXPFCXMLTag_tabwidget:
      aClass = kCXPFCTabWidgetCID;
      break;
      
    default:
      return (NS_ERROR_UNEXPECTED);
      break;
  }


  return NS_OK;
}

NS_IMETHODIMP nsXPFCXMLContentSink::ConsumeAttributes(const nsIParserNode& aNode, nsIXMLParserObject& aObject)
{  
  PRInt32 i = 0;
  nsString key,value;
  nsString scontainer;
  PRBool container = PR_FALSE;

  for (i = 0; i < aNode.GetAttributeCount(); i++) {

   key   = aNode.GetKeyAt(i);
   value = aNode.GetValueAt(i);

   key.StripChars("\"");
   value.StripChars("\"");

   aObject.SetParameter(key,value);

   if (key.EqualsIgnoreCase(XPFC_STRING_CONTAINER)) {
     container = PR_TRUE;
     scontainer = value;
   }

  }


  eXPFCXMLTags tag = (eXPFCXMLTags) aNode.GetNodeType();

  //if (container == PR_TRUE)
  //  aObject.SetParameter(nsString("popup"),nsString("popup"));

  /*
   * If this object has something it wants to control, store away the string and source
   * object.  Later on, we'll walk the content tree Registering appropriate subjects
   * and observers.
   */

  if (container == PR_TRUE)
  {
    mContainerList->Append(new ContainerListEntry(&aObject, scontainer));
  }

  return NS_OK;
}

PRBool nsXPFCXMLContentSink::IsContainer(const nsIParserNode& aNode)
{  
  PRInt32 i = 0;
  nsString key;
  PRBool container = PR_FALSE;

  for (i = 0; i < aNode.GetAttributeCount(); i++) {

   key   = aNode.GetKeyAt(i);
   key.StripChars("\"");

   if (key.EqualsIgnoreCase(XPFC_STRING_CONTAINER)) {
     container = PR_TRUE;
     break;
   }

  }

  return container;
}


NS_IMETHODIMP nsXPFCXMLContentSink::AddToHierarchy(nsIXMLParserObject& aObject, PRBool aPush)
{  

  if (mState == XPFC_PARSING_STATE_TOOLBAR)
  {

    nsIXPFCToolbar * container  = nsnull;
    nsIXPFCToolbar * parent = nsnull;
    nsIXPFCCanvas * child_canvas  = nsnull;

    aObject.QueryInterface(kIXPFCCanvasIID,(void**)&child_canvas);

    nsresult res = aObject.QueryInterface(kCIXPFCToolbarIID,(void**)&container);

    parent = (nsIXPFCToolbar *) mXPFCStack->Top();

    if (parent == nsnull)
    {
      mViewerContainer->GetToolbarManager()->AddToolbar(container);

    } else {

      nsIXPFCCanvas * parent_canvas = nsnull;

      if (child_canvas != nsnull)
      {
        res = parent->QueryInterface(kIXPFCCanvasIID,(void**)&parent_canvas);

        if (NS_OK == res)
        {
          parent_canvas->AddChildCanvas(child_canvas);
        
          NS_RELEASE(parent_canvas);
        }

      }

    }


    if (aPush == PR_TRUE)
      mXPFCStack->Push(child_canvas);

    NS_IF_RELEASE(child_canvas);

    return NS_OK;

  } else if (mState == XPFC_PARSING_STATE_MENUBAR)
  {

    nsIXPFCMenuContainer * container  = nsnull;
    nsIXPFCMenuContainer * parent = nsnull;

    nsresult res = aObject.QueryInterface(kCIXPFCMenuContainerIID,(void**)&container);

    parent = (nsIXPFCMenuContainer *) mXPFCStack->Top();

    if (NS_OK != res)
    {

      /*
       *  Must be a menu item.  Add as child to top of stack
       */
      nsIXPFCMenuItem * item  = nsnull;

      aObject.QueryInterface(kCIXPFCMenuItemIID,(void**)&item);

      parent->AddChild(item);

      item->SetMenuID(mViewerContainer->GetMenuManager()->GetID());
      item->SetReceiver(mViewerContainer->GetMenuManager()->GetDefaultReceiver());

      NS_RELEASE(item);

      return res;
    }


    if (res == NS_OK && parent != nsnull)
    {
      nsIXPFCMenuItem * item  = nsnull;

      nsresult res = aObject.QueryInterface(kCIXPFCMenuItemIID,(void**)&item);

      parent->AddChild(item);

      item->SetMenuID(mViewerContainer->GetMenuManager()->GetID());
      item->SetReceiver(mViewerContainer->GetMenuManager()->GetDefaultReceiver());

      NS_RELEASE(item);
    }

    if (parent == nsnull)
    {
      /*
       * Add all toplevel menucontainer's to the menu manager
       */

      mViewerContainer->GetMenuManager()->AddMenuContainer(container);

      /*
       * We probably can get rid of the orphanlist now that we
       * have a menu manager.
       */

      mOrphanMenuList->Append(container);

      
    }

    if (aPush == PR_TRUE)
      mXPFCStack->Push(container);

    
    container->Release();

    return NS_OK;
  } else if (mState == XPFC_PARSING_STATE_DIALOG)
  {

    nsIXPFCDialog * container  = nsnull;
    nsIXPFCDialog * parent = nsnull;

    nsresult res = aObject.QueryInterface(kCIXPFCDialogIID,(void**)&container);

    parent = (nsIXPFCDialog *) mXPFCStack->Top();

    if (parent == nsnull)
    {
      //mViewerContainer->GetToolbarManager()->AddDialog(container);

      if (aPush == PR_TRUE)
        mXPFCStack->Push(container);

    } else {

      nsIXPFCCanvas * parent_canvas = nsnull;
      nsIXPFCCanvas * child_canvas  = nsnull;

      res = aObject.QueryInterface(kIXPFCCanvasIID,(void**)&child_canvas);

      if (NS_OK == res)
      {
        res = parent->QueryInterface(kIXPFCCanvasIID,(void**)&parent_canvas);

        if (NS_OK == res)
        {
          parent_canvas->AddChildCanvas(child_canvas);
        
          //NS_RELEASE(parent_canvas);
        }

        if (aPush == PR_TRUE)
          mXPFCStack->Push(child_canvas);

        //NS_RELEASE(child_canvas);
      }

    }

    return NS_OK;

  } else if (mState == XPFC_PARSING_STATE_APPLICATION)
  {

    nsIXPFCCanvas * container  = nsnull;
    nsIXPFCCanvas * parent = nsnull;

    nsresult res = aObject.QueryInterface(kIXPFCCanvasIID,(void**)&container);

    parent = (nsIXPFCCanvas *) mXPFCStack->Top();

    if (parent == nsnull)
    {
      if (aPush == PR_TRUE)
        mXPFCStack->Push(container);

    } else {

      parent->AddChildCanvas(container);

      if (aPush == PR_TRUE)
        mXPFCStack->Push(container);
    }

    return NS_OK;

  } else {

    return NS_OK;
  }

  return NS_OK;
}


nsresult nsXPFCXMLContentSink::SetRootCanvas(nsIXPFCCanvas * aCanvas)
{
  mXPFCStack->Push(aCanvas);
  return NS_OK;
}
