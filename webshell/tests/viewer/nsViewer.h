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
#ifndef nsViewer_h___
#define nsViewer_h___

#include "nsIWebWidget.h"
#include "nsIDocumentObserver.h"
#include "nsIStreamListener.h"
#include "nsILinkHandler.h"
#include "nsIDocumentLoader.h"
#include "nsDocLoader.h"
#include "nsIAppShell.h"
#include "nsString.h"
#include "nsINetContainerApplication.h"

#ifdef XP_PC
#define WIDGET_DLL "raptorwidget.dll"
#define GFXWIN_DLL "raptorgfxwin.dll"
#define VIEW_DLL   "raptorview.dll"
#define WEB_DLL    "raptorweb.dll"
#else
#define WIDGET_DLL "libwidgetunix.so"
#define GFXWIN_DLL "libgfxunix.so"
#define VIEW_DLL   "libraptorview.so"
#define WEB_DLL    "libraptorwebwidget.so"
#endif

#define MAXPATHLEN 1024

// XXX this is redundant with nsLinkHandler
class DocObserver : public nsIDocumentObserver,
                    public nsIStreamObserver,
                    public nsILinkHandler,
                    public nsIViewerContainer
{
public:
  DocObserver(nsIWidget* aWindow, nsIWebWidget* aWebWidget);

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDocumentObserver
  NS_IMETHOD SetTitle(const nsString& aTitle);
  NS_IMETHOD BeginUpdate();
  NS_IMETHOD EndUpdate();
  NS_IMETHOD BeginLoad();
  NS_IMETHOD EndLoad();
  NS_IMETHOD BeginReflow(nsIPresShell* aShell);
  NS_IMETHOD EndReflow(nsIPresShell* aShell);
  NS_IMETHOD ContentChanged(nsIContent* aContent,
                            nsISupports* aSubContent);
  NS_IMETHOD ContentAppended(nsIContent* aContainer);
  NS_IMETHOD ContentInserted(nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentReplaced(nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentWillBeRemoved(nsIContent* aContainer,
                                  nsIContent* aChild,
                                  PRInt32 aIndexInContainer);
  NS_IMETHOD ContentHasBeenRemoved(nsIContent* aContainer,
                                   nsIContent* aChild,
                                   PRInt32 aIndexInContainer);
  NS_IMETHOD StyleSheetAdded(nsIStyleSheet* aStyleSheet);

  // nsIStreamObserver
  NS_IMETHOD OnStartBinding(const char *aContentType);
  NS_IMETHOD OnProgress(PRInt32 aProgress, PRInt32 aProgressMax,
                        const nsString& aMsg);
  NS_IMETHOD OnStopBinding(PRInt32 status, const nsString& aMsg);

  // nsILinkHandler
  NS_IMETHOD Init(nsIWebWidget* aWidget);
  NS_IMETHOD GetWebWidget(nsIWebWidget** aResult);
  NS_IMETHOD OnLinkClick(nsIFrame* aFrame, 
                         const nsString& aURLSpec,
                         const nsString& aTargetSpec,
                         nsIPostData* aPostData = 0);
  NS_IMETHOD OnOverLink(nsIFrame* aFrame, 
                        const nsString& aURLSpec,
                        const nsString& aTargetSpec);
  NS_IMETHOD GetLinkState(const nsString& aURLSpec, nsLinkState& aState);

  // nsIViewerContainer
  NS_IMETHOD Embed(nsIDocumentWidget* aDocViewer, 
                   const char* aCommand, 
                   nsISupports* aExtraInfo);

  // DocObserver
  void LoadURL(const char* aURL);
  void HandleLinkClickEvent(const nsString& aURLSpec,
                            const nsString& aTargetSpec,
                            nsIPostData* aPostDat = 0);

  nsIWebWidget* mWebWidget;
  nsIWidget *mWindowWidget;

protected:
  virtual ~DocObserver();

  nsIDocumentLoader* mDocLoader;
  nsString mURL;
  nsString mOverURL;
  nsString mOverTarget;
};

struct WindowData {
///  nsIWebWidget* ww;
  DocObserver* observer;
  nsIWidget *windowWidget;

  WindowData() {
///    ww = nsnull;
  }

  void ShowContentSize();
  void ShowFrameSize();
  void ShowStyleSize();
};

class nsViewer : public nsINetContainerApplication, public nsDispatchListener {
  public:
    // nsISupports
    NS_DECL_ISUPPORTS

    virtual char* GetBaseURL();
    virtual char* GetDefaultStartURL();
    virtual void AddMenu(nsIWidget* aMainWindow, PRBool aForPrintPreview);
    virtual void ShowConsole(WindowData* aWindata);
    virtual void CloseConsole();
    virtual void DoDebugRobot(WindowData* aWindata);
    virtual void CopySelection(WindowData* aWindata);
    virtual nsresult Run();
    virtual void Destroy(WindowData* wd);
    virtual void Stop();
    virtual void AfterDispatch();
    virtual void CrtSetDebug(PRUint32 aNewFlags);
    virtual void ExitViewer();


    virtual nsDocLoader* SetupViewer(nsIWidget **aMainWindow,int argc, char **argv);
    virtual void CleanupViewer(nsDocLoader* aDl);
    virtual nsEventStatus DispatchMenuItem(nsGUIEvent *aEvent);
    virtual nsEventStatus DispatchMenuItem(PRUint32 aId);
    virtual nsEventStatus ProcessMenu(PRUint32 aId, WindowData* wd);

    virtual nsresult ShowPrintPreview(nsIWebWidget* web, PRIntn aColumns);
    virtual WindowData* CreateTopLevel(const char* title, int aWidth, int aHeight);
    virtual void AddTestDocs(nsDocLoader* aDocLoader);
    virtual void AddTestDocsFromFile(nsDocLoader* aDocLoader, char *aFileName);
    virtual void DestroyAllWindows();
    virtual struct WindowData* FindWindowData(nsIWidget* aWidget);
    virtual PRBool GetFileNameFromFileSelector(nsIWidget* aParentWindow, nsString* aFileName);
    virtual void OpenHTMLFile(WindowData* wd);
    virtual void SelectAll(WindowData* wd);
    virtual void ProcessArguments(int argc, char **argv);

    NS_IMETHOD    GetAppCodeName(nsString& aAppCodeName);
    NS_IMETHOD    GetAppVersion(nsString& aAppVersion);
    NS_IMETHOD    GetAppName(nsString& aAppName);
    NS_IMETHOD    GetLanguage(nsString& aLanguage);
    NS_IMETHOD    GetPlatform(nsString& aPlatform);
};

  // Set the single viewer.
extern void SetViewer(nsViewer* aViewer);


#endif
