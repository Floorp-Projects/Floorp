/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMEditorAppCore_h__
#define nsIDOMEditorAppCore_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMBaseAppCore.h"

class nsIDOMElement;
class nsIDOMDocument;
class nsIDOMSelection;
class nsIDOMWindow;

#define NS_IDOMEDITORAPPCORE_IID \
 { 0x9afff72b, 0xca9a, 0x11d2, \
    {0x96, 0xc9, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56}} 

class nsIDOMEditorAppCore : public nsIDOMBaseAppCore {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMEDITORAPPCORE_IID; return iid; }

  NS_IMETHOD    GetContentsAsText(nsString& aContentsAsText)=0;

  NS_IMETHOD    GetContentsAsHTML(nsString& aContentsAsHTML)=0;

  NS_IMETHOD    GetEditorDocument(nsIDOMDocument** aEditorDocument)=0;

  NS_IMETHOD    GetEditorSelection(nsIDOMSelection** aEditorSelection)=0;

  NS_IMETHOD    GetParagraphFormat(nsString& aParagraphFormat)=0;

  NS_IMETHOD    GetWrapColumn(PRInt32* aWrapColumn)=0;
  NS_IMETHOD    SetWrapColumn(PRInt32 aWrapColumn)=0;

  NS_IMETHOD    SetEditorType(const nsString& aEditorType)=0;

  NS_IMETHOD    SetTextProperty(const nsString& aProp, const nsString& aAttr, const nsString& aValue)=0;

  NS_IMETHOD    RemoveTextProperty(const nsString& aProp, const nsString& aAttr)=0;

  NS_IMETHOD    GetTextProperty(const nsString& aProp, const nsString& aAttr, const nsString& aValue, nsString& aFirstHas, nsString& aAnyHas, nsString& aAllHas)=0;

  NS_IMETHOD    SetParagraphFormat(const nsString& aValue)=0;

  NS_IMETHOD    LoadUrl(const nsString& aUrl)=0;

  NS_IMETHOD    NewWindow()=0;

  NS_IMETHOD    Open()=0;

  NS_IMETHOD    Save()=0;

  NS_IMETHOD    SaveAs()=0;

  NS_IMETHOD    CloseWindow()=0;

  NS_IMETHOD    Print()=0;

  NS_IMETHOD    Exit()=0;

  NS_IMETHOD    Undo()=0;

  NS_IMETHOD    Redo()=0;

  NS_IMETHOD    Cut()=0;

  NS_IMETHOD    Copy()=0;

  NS_IMETHOD    Paste()=0;

  NS_IMETHOD    SelectAll()=0;

  NS_IMETHOD    Find(const nsString& aSearchTerm, PRBool aMatchCase, PRBool aSearchDown)=0;

  NS_IMETHOD    BeginBatchChanges()=0;

  NS_IMETHOD    EndBatchChanges()=0;

  NS_IMETHOD    InsertText(const nsString& aTextToInsert)=0;

  NS_IMETHOD    InsertLink()=0;

  NS_IMETHOD    InsertImage()=0;

  NS_IMETHOD    GetSelectedElement(const nsString& aTagName, nsIDOMElement** aReturn)=0;

  NS_IMETHOD    CreateElementWithDefaults(const nsString& aTagName, nsIDOMElement** aReturn)=0;

  NS_IMETHOD    InsertElement(nsIDOMElement* aElement, PRBool aDeleteSelection, nsIDOMElement** aReturn)=0;

  NS_IMETHOD    InsertLinkAroundSelection(nsIDOMElement* aAnchorElement)=0;

  NS_IMETHOD    SetToolbarWindow(nsIDOMWindow* aWin)=0;

  NS_IMETHOD    SetContentWindow(nsIDOMWindow* aWin)=0;

  NS_IMETHOD    SetWebShellWindow(nsIDOMWindow* aWin)=0;
};


#define NS_DECL_IDOMEDITORAPPCORE   \
  NS_IMETHOD    GetContentsAsText(nsString& aContentsAsText);  \
  NS_IMETHOD    GetContentsAsHTML(nsString& aContentsAsHTML);  \
  NS_IMETHOD    GetEditorDocument(nsIDOMDocument** aEditorDocument);  \
  NS_IMETHOD    GetEditorSelection(nsIDOMSelection** aEditorSelection);  \
  NS_IMETHOD    GetParagraphFormat(nsString& aParagraphFormat);  \
  NS_IMETHOD    GetWrapColumn(PRInt32* aWrapColumn);  \
  NS_IMETHOD    SetWrapColumn(PRInt32 aWrapColumn);  \
  NS_IMETHOD    SetEditorType(const nsString& aEditorType);  \
  NS_IMETHOD    SetTextProperty(const nsString& aProp, const nsString& aAttr, const nsString& aValue);  \
  NS_IMETHOD    RemoveTextProperty(const nsString& aProp, const nsString& aAttr);  \
  NS_IMETHOD    GetTextProperty(const nsString& aProp, const nsString& aAttr, const nsString& aValue, nsString& aFirstHas, nsString& aAnyHas, nsString& aAllHas);  \
  NS_IMETHOD    SetParagraphFormat(const nsString& aValue);  \
  NS_IMETHOD    LoadUrl(const nsString& aUrl);  \
  NS_IMETHOD    NewWindow();  \
  NS_IMETHOD    Open();  \
  NS_IMETHOD    Save();  \
  NS_IMETHOD    SaveAs();  \
  NS_IMETHOD    CloseWindow();  \
  NS_IMETHOD    Print();  \
  NS_IMETHOD    Exit();  \
  NS_IMETHOD    Undo();  \
  NS_IMETHOD    Redo();  \
  NS_IMETHOD    Cut();  \
  NS_IMETHOD    Copy();  \
  NS_IMETHOD    Paste();  \
  NS_IMETHOD    SelectAll();  \
  NS_IMETHOD    Find(const nsString& aSearchTerm, PRBool aMatchCase, PRBool aSearchDown);  \
  NS_IMETHOD    BeginBatchChanges();  \
  NS_IMETHOD    EndBatchChanges();  \
  NS_IMETHOD    InsertText(const nsString& aTextToInsert);  \
  NS_IMETHOD    InsertLink();  \
  NS_IMETHOD    InsertImage();  \
  NS_IMETHOD    GetSelectedElement(const nsString& aTagName, nsIDOMElement** aReturn);  \
  NS_IMETHOD    CreateElementWithDefaults(const nsString& aTagName, nsIDOMElement** aReturn);  \
  NS_IMETHOD    InsertElement(nsIDOMElement* aElement, PRBool aDeleteSelection, nsIDOMElement** aReturn);  \
  NS_IMETHOD    InsertLinkAroundSelection(nsIDOMElement* aAnchorElement);  \
  NS_IMETHOD    SetToolbarWindow(nsIDOMWindow* aWin);  \
  NS_IMETHOD    SetContentWindow(nsIDOMWindow* aWin);  \
  NS_IMETHOD    SetWebShellWindow(nsIDOMWindow* aWin);  \



#define NS_FORWARD_IDOMEDITORAPPCORE(_to)  \
  NS_IMETHOD    GetContentsAsText(nsString& aContentsAsText) { return _to GetContentsAsText(aContentsAsText); } \
  NS_IMETHOD    GetContentsAsHTML(nsString& aContentsAsHTML) { return _to GetContentsAsHTML(aContentsAsHTML); } \
  NS_IMETHOD    GetEditorDocument(nsIDOMDocument** aEditorDocument) { return _to GetEditorDocument(aEditorDocument); } \
  NS_IMETHOD    GetEditorSelection(nsIDOMSelection** aEditorSelection) { return _to GetEditorSelection(aEditorSelection); } \
  NS_IMETHOD    GetParagraphFormat(nsString& aParagraphFormat) { return _to GetParagraphFormat(aParagraphFormat); } \
  NS_IMETHOD    GetWrapColumn(PRInt32* aWrapColumn) { return _to GetWrapColumn(aWrapColumn); } \
  NS_IMETHOD    SetWrapColumn(PRInt32 aWrapColumn) { return _to SetWrapColumn(aWrapColumn); } \
  NS_IMETHOD    SetEditorType(const nsString& aEditorType) { return _to SetEditorType(aEditorType); }  \
  NS_IMETHOD    SetTextProperty(const nsString& aProp, const nsString& aAttr, const nsString& aValue) { return _to SetTextProperty(aProp, aAttr, aValue); }  \
  NS_IMETHOD    RemoveTextProperty(const nsString& aProp, const nsString& aAttr) { return _to RemoveTextProperty(aProp, aAttr); }  \
  NS_IMETHOD    GetTextProperty(const nsString& aProp, const nsString& aAttr, const nsString& aValue, nsString& aFirstHas, nsString& aAnyHas, nsString& aAllHas) { return _to GetTextProperty(aProp, aAttr, aValue, aFirstHas, aAnyHas, aAllHas); }  \
  NS_IMETHOD    SetParagraphFormat(const nsString& aValue) { return _to SetParagraphFormat(aValue); }  \
  NS_IMETHOD    LoadUrl(const nsString& aUrl) { return _to LoadUrl(aUrl); }  \
  NS_IMETHOD    NewWindow() { return _to NewWindow(); }  \
  NS_IMETHOD    Open() { return _to Open(); }  \
  NS_IMETHOD    Save() { return _to Save(); }  \
  NS_IMETHOD    SaveAs() { return _to SaveAs(); }  \
  NS_IMETHOD    CloseWindow() { return _to CloseWindow(); }  \
  NS_IMETHOD    Print() { return _to Print(); }  \
  NS_IMETHOD    Exit() { return _to Exit(); }  \
  NS_IMETHOD    Undo() { return _to Undo(); }  \
  NS_IMETHOD    Redo() { return _to Redo(); }  \
  NS_IMETHOD    Cut() { return _to Cut(); }  \
  NS_IMETHOD    Copy() { return _to Copy(); }  \
  NS_IMETHOD    Paste() { return _to Paste(); }  \
  NS_IMETHOD    SelectAll() { return _to SelectAll(); }  \
  NS_IMETHOD    Find(const nsString& aSearchTerm, PRBool aMatchCase, PRBool aSearchDown) { return _to Find(aSearchTerm, aMatchCase, aSearchDown); }  \
  NS_IMETHOD    BeginBatchChanges() { return _to BeginBatchChanges(); }  \
  NS_IMETHOD    EndBatchChanges() { return _to EndBatchChanges(); }  \
  NS_IMETHOD    InsertText(const nsString& aTextToInsert) { return _to InsertText(aTextToInsert); }  \
  NS_IMETHOD    InsertLink() { return _to InsertLink(); }  \
  NS_IMETHOD    InsertImage() { return _to InsertImage(); }  \
  NS_IMETHOD    GetSelectedElement(const nsString& aTagName, nsIDOMElement** aReturn) { return _to GetSelectedElement(aTagName, aReturn); }  \
  NS_IMETHOD    CreateElementWithDefaults(const nsString& aTagName, nsIDOMElement** aReturn) { return _to CreateElementWithDefaults(aTagName, aReturn); }  \
  NS_IMETHOD    InsertElement(nsIDOMElement* aElement, PRBool aDeleteSelection, nsIDOMElement** aReturn) { return _to InsertElement(aElement, aDeleteSelection, aReturn); }  \
  NS_IMETHOD    InsertLinkAroundSelection(nsIDOMElement* aAnchorElement) { return _to InsertLinkAroundSelection(aAnchorElement); }  \
  NS_IMETHOD    SetToolbarWindow(nsIDOMWindow* aWin) { return _to SetToolbarWindow(aWin); }  \
  NS_IMETHOD    SetContentWindow(nsIDOMWindow* aWin) { return _to SetContentWindow(aWin); }  \
  NS_IMETHOD    SetWebShellWindow(nsIDOMWindow* aWin) { return _to SetWebShellWindow(aWin); }  \


extern "C" NS_DOM nsresult NS_InitEditorAppCoreClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptEditorAppCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMEditorAppCore_h__
