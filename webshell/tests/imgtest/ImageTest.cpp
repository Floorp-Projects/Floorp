/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
//we need openfilename stuff... MMP
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "prtypes.h"
#include <stdio.h>
#include "resources.h"
#include "nsIImageManager.h"
#include "nsIImageGroup.h"
#include "nsIImageRequest.h"
#include "nsIImageObserver.h"
#include "nsIRenderingContext.h"
#include "nsIImage.h"
#include "nsIWidget.h"
#include "nsGUIEvent.h"
#include "nsRect.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"
#include "nsFileSpec.h"
#include "nsIDeviceContext.h"
#include "nsIServiceManager.h"

static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kCWindowIID, NS_WINDOW_CID);
static NS_DEFINE_IID(kCChildWindowIID, NS_CHILD_CID);
static NS_DEFINE_IID(kCScrollbarIID, NS_VERTSCROLLBAR_CID);
static NS_DEFINE_IID(kImageManagerCID, NS_IMAGEMANAGER_CID);

static char* class1Name = "ImageTest";

static HINSTANCE gInstance, gPrevInstance;
static nsCOMPtr<nsIImageManager> gImageManager;
static nsIImageGroup *gImageGroup = nsnull;
static nsIImageRequest *gImageReq = nsnull;
static HWND gHwnd;
static nsIWidget *gWindow = nsnull;
static nsIImage *gImage = nsnull;
static PRBool gInstalledColorMap = PR_FALSE;

class MyObserver : public nsIImageRequestObserver {
public:
    MyObserver();
    ~MyObserver();
 
    NS_DECL_ISUPPORTS

    virtual void Notify(nsIImageRequest *aImageRequest,
                        nsIImage *aImage,
                        nsImageNotification aNotificationType,
                        PRInt32 aParam1, PRInt32 aParam2,
                        void *aParam3);

    virtual void NotifyError(nsIImageRequest *aImageRequest,
                             nsImageError aErrorType);
};

MyObserver::MyObserver()
{
}

MyObserver::~MyObserver()
{
}

NS_IMPL_ISUPPORTS1(MyObserver, nsIImageRequestObserver)

void  
MyObserver::Notify(nsIImageRequest *aImageRequest,
                   nsIImage *aImage,
                   nsImageNotification aNotificationType,
                   PRInt32 aParam1, PRInt32 aParam2,
                   void *aParam3)
{
    switch (aNotificationType) {
       case nsImageNotification_kDimensions:
       {
           char buffer[40];
           sprintf(buffer, "Image:%d x %d", aParam1, aParam2);
           ::SetWindowText(gHwnd, buffer);
       }
       break;

       case nsImageNotification_kPixmapUpdate:
       case nsImageNotification_kImageComplete:
       case nsImageNotification_kFrameComplete:
       {
           if (gImage == nsnull && aImage) {
               gImage = aImage;
               NS_ADDREF(aImage);
           }

           if (!gInstalledColorMap && gImage) {
               nsColorMap *cmap = gImage->GetColorMap();

               if (cmap != nsnull && cmap->NumColors > 0) {
                 gWindow->SetColorMap(cmap);
               }
               gInstalledColorMap = PR_TRUE;
           }
           nsRect *rect = (nsRect *)aParam3;
           nsIRenderingContext *drawCtx = gWindow->GetRenderingContext();
           
           if (gImage) {
               drawCtx->DrawImage(gImage, 0, 0, 
                                  gImage->GetWidth(), gImage->GetHeight());
           }
       }
       break;
    }
}

void 
MyObserver::NotifyError(nsIImageRequest *aImageRequest,
                        nsImageError aErrorType)
{
  ::MessageBox(NULL, "Image loading error!",
               class1Name, MB_OK);
}

nsEventStatus PR_CALLBACK 
MyHandleEvent(nsGUIEvent *aEvent)
{
    nsEventStatus result = nsEventStatus_eConsumeNoDefault;
    switch(aEvent->message) {
        case NS_PAINT:
        {
            // paint the background
            nsIRenderingContext *drawCtx = ((nsPaintEvent*)aEvent)->renderingContext;
            drawCtx->SetColor(aEvent->widget->GetBackgroundColor());
            drawCtx->FillRect(*(((nsPaintEvent*)aEvent)->rect));

            if (gImage) {
                drawCtx->DrawImage(gImage, 0, 0, 
                                   gImage->GetWidth(), gImage->GetHeight());
            }
            
            return nsEventStatus_eConsumeNoDefault;
        }
        break;
    }

    return nsEventStatus_eIgnore; 
}

void
MyReleaseImages()
{
    if (gImageReq) {
        NS_RELEASE(gImageReq);
        gImageReq = NULL;
    }
    if (gImage) {
        NS_RELEASE(gImage);
        gImage = NULL;
    }

    gInstalledColorMap = PR_FALSE;
}

void
MyInterrupt()
{
    if (gImageGroup) {
        gImageGroup->Interrupt();
    }
}

#define FILE_URL_PREFIX "file://"


void
MyLoadImage(char *aFileName)
{
    char fileURL[256];
    char *str;

    MyInterrupt();
    MyReleaseImages();

    if (gImageGroup == NULL) {
        nsIDeviceContext *deviceCtx = gWindow->GetDeviceContext();
        if (NS_NewImageGroup(&gImageGroup) != NS_OK ||
            gImageGroup->Init(deviceCtx, nsnull) != NS_OK)
        {
                ::MessageBox(NULL, "Couldn't create image group",
                             class1Name, MB_OK);
                NS_RELEASE(deviceCtx);
                return;
            }
        NS_RELEASE(deviceCtx);
    }

    strcpy(fileURL, FILE_URL_PREFIX);
    strcpy(fileURL + strlen(FILE_URL_PREFIX), aFileName);

    str = fileURL;
    while ((str = strchr(str, '\\')) != NULL)
        *str = '/';

    nscolor white;
    MyObserver *observer = new MyObserver();
            
    NS_ColorNameToRGB(NS_LITERAL_STRING("white"), &white);
    gImageReq = gImageGroup->GetImage(fileURL,
                                      observer,
                                      &white, 0, 0, 0);
    if (gImageReq == NULL) {
      ::MessageBox(NULL, "Couldn't create image request",
                   class1Name, MB_OK);
    }
}

PRBool
OpenFileDialog(char *aBuffer, int32 aBufLen)
{
    BOOL result = FALSE;
    OPENFILENAME ofn;

    // *.js is the standard File Name on the Save/Open Dialog
    ::strcpy(aBuffer, "*.gif;*.png;*.jpg;*.jpeg");

    // fill the OPENFILENAME sruct
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = gHwnd;
    ofn.hInstance = gInstance;
    ofn.lpstrFilter = "All Images (*.gif,*.png,*.jpg,*.jpeg)\0*.gif;*png;*.jpg;*.jpeg\0GIF Files (*.gif)\0*.gif\0PNG Files (*.png)\0*.png\0JPEG Files (*.jpg,*.jpeg)\0*.jpg;*.jpeg\0All Files\0*.*\0\0";
    ofn.lpstrCustomFilter = NULL; 
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1; // the first one in lpstrFilter
    ofn.lpstrFile = aBuffer; // contains the file path name on return
    ofn.nMaxFile = aBufLen;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL; // use default
    ofn.lpstrTitle = NULL; // use default
    ofn.Flags = OFN_CREATEPROMPT | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
    ofn.nFileOffset = 0; 
    ofn.nFileExtension = 0;
    ofn.lpstrDefExt = "gif"; // default extension is .js
    ofn.lCustData = NULL; 
    ofn.lpfnHook = NULL;
    ofn.lpTemplateName = NULL;

    // call the open file dialog or the save file dialog according to aIsOpenDialog
    result = ::GetOpenFileName(&ofn);

    return (PRBool)result;
}

long PASCAL
WndProc(HWND hWnd, UINT msg, WPARAM param, LPARAM lparam)
{
  HMENU hMenu;

  switch (msg) {
    case WM_COMMAND:
      hMenu = GetMenu(hWnd);

      switch (LOWORD(param)) {
        case TIMER_OPEN: 
        {
            char szFile[256];

            if (!OpenFileDialog(szFile, 256))
                return 0L;

            MyLoadImage(szFile);
            break;
        }
        case TIMER_EXIT:
            ::DestroyWindow(hWnd);
            exit(0);
        default:
            break;
      }
      break;

    case WM_CREATE:
      // Initialize image library
      { nsresult result;
        gImageManager = do_GetService(kImageManagerCID, &result);
        if ((NS_FAILED(result)) || gImageManager->Init() != NS_OK)
          {
          ::MessageBox(NULL, "Can't initialize the image library",class1Name, MB_OK);
          }	 
        else
          gImageManager->SetCacheSize(1024*1024);
      }
      break;

    case WM_DESTROY:
      MyInterrupt();
      MyReleaseImages();
      if (gImageGroup != nsnull) {
          NS_RELEASE(gImageGroup);
      }
      gImageManager = nsnull;
      PostQuitMessage(0);
      break;

    default:
      break;
  }

  return DefWindowProc(hWnd, msg, param, lparam);
}

static HWND CreateTopLevel(const char* clazz, const char* title,
                                  int aWidth, int aHeight)
{
  // Create a simple top level window
  HWND window = ::CreateWindowEx(WS_EX_CLIENTEDGE,
                                 clazz, title,
                                 WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
                                 CW_USEDEFAULT, CW_USEDEFAULT,
                                 aWidth, aHeight,
                                 HWND_DESKTOP,
                                 NULL,
                                 gInstance,
                                 NULL);

  nsRect rect(0, 0, aWidth, aHeight);  

  nsresult rv = nsComponentManager::CreateInstance(kCChildWindowIID, NULL, kIWidgetIID, (void**)&gWindow);

  if (NS_OK == rv) {
      gWindow->Create((nsNativeWidget)window, rect, MyHandleEvent, NULL);
  }

  ::ShowWindow(window, SW_SHOW);
  ::UpdateWindow(window);
  return window;
}

#define WIDGET_DLL "gkwidget.dll"
#define GFXWIN_DLL "gkgfxwin.dll"

int PASCAL
WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdParam, int nCmdShow)
{
  gInstance = instance;

  nsComponentManager::RegisterComponent(kCWindowIID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCChildWindowIID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCScrollbarIID, NULL, NULL, WIDGET_DLL, PR_FALSE, PR_FALSE);

  static NS_DEFINE_IID(kCRenderingContextIID, NS_RENDERING_CONTEXT_CID);
  static NS_DEFINE_IID(kCDeviceContextIID, NS_DEVICE_CONTEXT_CID);
  static NS_DEFINE_IID(kCFontMetricsIID, NS_FONT_METRICS_CID);
  static NS_DEFINE_IID(kCImageIID, NS_IMAGE_CID);

  nsComponentManager::RegisterComponent(kCRenderingContextIID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCDeviceContextIID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCFontMetricsIID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kCImageIID, NULL, NULL, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponentLib(kImageManagerCID, "Image Manager", "@mozilla.org/gfx/imagemanager;1", GFXWIN_DLL, PR_FALSE, PR_FALSE);

  if (!prevInstance) {
    WNDCLASS wndClass;
    wndClass.style = 0;
    wndClass.lpfnWndProc = WndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = gInstance;
    wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH) GetStockObject(LTGRAY_BRUSH);
    wndClass.lpszMenuName = class1Name;
    wndClass.lpszClassName = class1Name;
    RegisterClass(&wndClass);
  }

  // Create our first top level window
  HWND gHwnd = CreateTopLevel(class1Name, "Raptor HTML Viewer", 620, 400);

  // Process messages
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
  }
  return msg.wParam;
}

void main(int argc, char **argv)
{
  WinMain(GetModuleHandle(NULL), NULL, 0, SW_SHOW);
}
