/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIContent.h"
#include "nsIPrincipal.h"
#include "nsIJSEventListener.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMXULDocument.h"
#include "nsINSEvent.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsIXULKeyListener.h"
#include "nsIXULDocument.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIXULPrototypeDocument.h"
#include "nsIScriptObjectOwner.h"
#include "nsIXULContentSink.h"
#include "nsRDFCID.h"
#include "nsINameSpaceManager.h"
#include "nsHashtable.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIPresContext.h"
#include "nsIDOMEventTarget.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"

#include "nsIObserver.h"
#include "nsIObserverService.h"

  enum {
    VK_CANCEL = 3,
    VK_BACK = 8,
    VK_TAB = 9,
    VK_CLEAR = 12,
    VK_RETURN = 13,
    VK_ENTER = 14,
    VK_SHIFT = 16,
    VK_CONTROL = 17,
    VK_ALT = 18,
    VK_PAUSE = 19,
    VK_CAPS_LOCK = 20,
    VK_ESCAPE = 27,
    VK_SPACE = 32,
    VK_PAGE_UP = 33,
    VK_PAGE_DOWN = 34,
    VK_END = 35,
    VK_HOME = 36,
    VK_LEFT = 37,
    VK_UP = 38,
    VK_RIGHT = 39,
    VK_DOWN = 40,
    VK_PRINTSCREEN = 44,
    VK_INSERT = 45,
    VK_DELETE = 46,
    VK_0 = 48,
    VK_1 = 49,
    VK_2 = 50,
    VK_3 = 51,
    VK_4 = 52,
    VK_5 = 53,
    VK_6 = 54,
    VK_7 = 55,
    VK_8 = 56,
    VK_9 = 57,
    VK_SEMICOLON = 59,
    VK_EQUALS = 61,
    VK_A = 65,
    VK_B = 66,
    VK_C = 67,
    VK_D = 68,
    VK_E = 69,
    VK_F = 70,
    VK_G = 71,
    VK_H = 72,
    VK_I = 73,
    VK_J = 74,
    VK_K = 75,
    VK_L = 76,
    VK_M = 77,
    VK_N = 78,
    VK_O = 79,
    VK_P = 80,
    VK_Q = 81,
    VK_R = 82,
    VK_S = 83,
    VK_T = 84,
    VK_U = 85,
    VK_V = 86,
    VK_W = 87,
    VK_X = 88,
    VK_Y = 89,
    VK_Z = 90,
    VK_NUMPAD0 = 96,
    VK_NUMPAD1 = 97,
    VK_NUMPAD2 = 98,
    VK_NUMPAD3 = 99,
    VK_NUMPAD4 = 100,
    VK_NUMPAD5 = 101,
    VK_NUMPAD6 = 102,
    VK_NUMPAD7 = 103,
    VK_NUMPAD8 = 104,
    VK_NUMPAD9 = 105,
    VK_MULTIPLY = 106,
    VK_ADD = 107,
    VK_SEPARATOR = 108,
    VK_SUBTRACT = 109,
    VK_DECIMAL = 110,
    VK_DIVIDE = 111,
    VK_F1 = 112,
    VK_F2 = 113,
    VK_F3 = 114,
    VK_F4 = 115,
    VK_F5 = 116,
    VK_F6 = 117,
    VK_F7 = 118,
    VK_F8 = 119,
    VK_F9 = 120,
    VK_F10 = 121,
    VK_F11 = 122,
    VK_F12 = 123,
    VK_F13 = 124,
    VK_F14 = 125,
    VK_F15 = 126,
    VK_F16 = 127,
    VK_F17 = 128,
    VK_F18 = 129,
    VK_F19 = 130,
    VK_F20 = 131,
    VK_F21 = 132,
    VK_F22 = 133,
    VK_F23 = 134,
    VK_F24 = 135,
    VK_NUM_LOCK = 144,
    VK_SCROLL_LOCK = 145,
    VK_COMMA = 188,
    VK_PERIOD = 190,
    VK_SLASH = 191,
    VK_BACK_QUOTE = 192,
    VK_OPEN_BRACKET = 219,
    VK_BACK_SLASH = 220,
    VK_CLOSE_BRACKET = 221,
    VK_QUOTE = 222
  };

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kXULKeyListenerCID,      NS_XULKEYLISTENER_CID);
static NS_DEFINE_CID(kXULDocumentCID, NS_XULDOCUMENT_CID);
static NS_DEFINE_CID(kXULContentSinkCID, NS_XULCONTENTSINK_CID);
static NS_DEFINE_CID(kParserCID,                 NS_PARSER_IID); // XXX

static NS_DEFINE_IID(kIXULKeyListenerIID,     NS_IXULKEYLISTENER_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kIDomNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDomElementIID,         NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDomEventListenerIID,   NS_IDOMEVENTLISTENER_IID);

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

enum eEventType {
  eKeyPress,
  eKeyDown,
  eKeyUp
};

class nsXULKeyBindingDeleter : public nsIObserver {
public:
  nsXULKeyBindingDeleter();
  virtual ~nsXULKeyBindingDeleter() { };

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIObserver
  NS_DECL_NSIOBSERVER
};

////////////////////////////////////////////////////////////////////////
// KeyListenerImpl
//
//   This is the key listener implementation for keybinding
//
class nsXULKeyListenerImpl : public nsIXULKeyListener,
                             public nsIDOMKeyListener
{
public:
    nsXULKeyListenerImpl(void);
    virtual ~nsXULKeyListenerImpl(void);

public:
    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIXULKeyListener
    NS_IMETHOD Init(
      nsIDOMElement  * aElement,
      nsIDOMDocument * aDocument);

    // to release the keybindings table from module shutdown
    static void Shutdown(void);

    // nsIDOMKeyListener

    /**
     * Processes a key pressed event
     * @param aKeyEvent @see nsIDOMEvent.h
     * @returns whether the event was consumed or ignored. @see nsresult
     */
    virtual nsresult KeyDown(nsIDOMEvent* aKeyEvent);

    /**
     * Processes a key release event
     * @param aKeyEvent @see nsIDOMEvent.h
     * @returns whether the event was consumed or ignored. @see nsresult
     */
    virtual nsresult KeyUp(nsIDOMEvent* aKeyEvent);

    /**
     * Processes a key typed event
     * @param aKeyEvent @see nsIDOMEvent.h
     * @returns whether the event was consumed or ignored. @see nsresult
     *
     */
    virtual nsresult KeyPress(nsIDOMEvent* aKeyEvent);

    // nsIDOMEventListener
    virtual nsresult HandleEvent(nsIDOMEvent* anEvent) { return NS_OK; };

protected:
    class nsIURIKey : public nsHashKey {
    protected:
        nsCOMPtr<nsIURI> mKey;

    public:
        nsIURIKey(nsIURI* key) : mKey(key) {}
        ~nsIURIKey(void) {}

        PRUint32 HashCode(void) const {
            nsXPIDLCString spec;
            mKey->GetSpec(getter_Copies(spec));
            return (PRUint32) PL_HashString(spec);
        }

        PRBool Equals(const nsHashKey *aKey) const {
            PRBool eq;
            mKey->Equals( ((nsIURIKey*) aKey)->mKey, &eq );
            return eq;
        }

        nsHashKey *Clone(void) const {
            return new nsIURIKey(mKey);
        }
    };

private:
    nsresult DoKey(nsIDOMEvent* aKeyEvent, eEventType aEventType);
    inline PRBool   IsMatchingKeyCode(const PRUint32 theChar, const nsString &keyName);
    inline PRBool   IsMatchingCharCode(const nsString &theChar, const nsString &keyName);

    NS_IMETHOD GetKeyBindingDocument(nsCAutoString& aURLStr, nsIDOMXULDocument** aResult);
    NS_IMETHOD LoadKeyBindingDocument(nsIURI* aURI, nsIDOMXULDocument** aResult);
    NS_IMETHOD LocateAndExecuteKeyBinding(nsIDOMKeyEvent* aKeyEvent, eEventType aEventType,
                                nsIDOMXULDocument* aDocument, PRBool& aHandled);
    NS_IMETHOD HandleEventUsingKeyset(nsIDOMElement* aKeysetElement, nsIDOMKeyEvent* aEvent, eEventType aEventType,
                                      nsIDOMXULDocument* aDocument, PRBool& aHandledFlag);

    nsIDOMElement* element; // Weak reference. The element will go away first.
    nsIDOMXULDocument* mDOMDocument; // Weak reference.

    static nsSupportsHashtable* mKeyBindingTable;

    // The "xul key" modifier is a key code from nsIDOMKeyEvent,
    // e.g. nsIDOMKeyEvent::DOM_VK_CONTROL
    PRUint32 mXULKeyModifier;
};

nsSupportsHashtable* nsXULKeyListenerImpl::mKeyBindingTable = nsnull;

class nsProxyStream : public nsIInputStream
{
private:
  const char* mBuffer;
  PRUint32    mSize;
  PRUint32    mIndex;

public:
  nsProxyStream(void) : mBuffer(nsnull)
  {
      NS_INIT_REFCNT();
  }

  virtual ~nsProxyStream(void) {
  }

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIBaseStream
  NS_IMETHOD Close(void) {
      return NS_OK;
  }

  // nsIInputStream
  NS_IMETHOD Available(PRUint32 *aLength) {
      *aLength = mSize - mIndex;
      return NS_OK;
  }

  NS_IMETHOD Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount) {
      PRUint32 readCount = 0;
      while (mIndex < mSize && aCount > 0) {
          *aBuf = mBuffer[mIndex];
          aBuf++;
          mIndex++;
          readCount++;
          aCount--;
      }
      *aReadCount = readCount;
      return NS_OK;
  }

  NS_IMETHOD ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval) {
    NS_NOTREACHED("ReadSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD GetNonBlocking(PRBool *aNonBlocking) {
    NS_NOTREACHED("GetNonBlocking");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD GetObserver(nsIInputStreamObserver * *aObserver) {
    NS_NOTREACHED("GetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD SetObserver(nsIInputStreamObserver * aObserver) {
    NS_NOTREACHED("SetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Implementation
  void SetBuffer(const char* aBuffer, PRUint32 aSize) {
      mBuffer = aBuffer;
      mSize = aSize;
      mIndex = 0;
  }
};

NS_IMPL_ISUPPORTS(nsProxyStream, NS_GET_IID(nsIInputStream));

////////////////////////////////////////////////////////////////////////

nsXULKeyBindingDeleter::nsXULKeyBindingDeleter(void)
{
  NS_INIT_ISUPPORTS();

  nsresult rv;
  NS_WITH_SERVICE (nsIObserverService, observerService, NS_OBSERVERSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) { return; }
  nsAutoString topic;
  topic.AssignWithConversion(NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  observerService->AddObserver(this,topic.GetUnicode()); 
}

NS_IMPL_ISUPPORTS1(nsXULKeyBindingDeleter, nsIObserver);

NS_IMETHODIMP
nsXULKeyBindingDeleter::Observe(nsISupports *aSubject, const PRUnichar *aTopic, const PRUnichar *someData)
{
#ifdef DEBUG
  nsAutoString topic;
  topic.AssignWithConversion(NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  NS_ASSERTION(topic.EqualsWithConversion(aTopic), "not shutdown");
#endif
  nsXULKeyListenerImpl::Shutdown();
  NS_RELEASE_THIS(); // we don't need to exist anymore
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////

nsXULKeyListenerImpl::nsXULKeyListenerImpl(void)
{
  NS_INIT_REFCNT();
  if (!mKeyBindingTable) {
    mKeyBindingTable = new nsSupportsHashtable();

    // XXX This is a total hack to deal with bug 27739. At some point,
    // the keybindings table will go away.
    nsXULKeyBindingDeleter *deleter = new nsXULKeyBindingDeleter();
    NS_IF_ADDREF(deleter); // this will delete itself, and the
                           // keybindings table, on XPCOM shutdown
  }
}

nsXULKeyListenerImpl::~nsXULKeyListenerImpl(void)
{
}

NS_IMPL_ADDREF(nsXULKeyListenerImpl)
NS_IMPL_RELEASE(nsXULKeyListenerImpl)

NS_IMETHODIMP
nsXULKeyListenerImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(NS_GET_IID(nsIXULKeyListener)) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIXULKeyListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (iid.Equals(NS_GET_IID(nsIDOMKeyListener))) {
        *result = NS_STATIC_CAST(nsIDOMKeyListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (iid.Equals(kIDomEventListenerIID)) {
        *result = (nsIDOMEventListener*)(nsIDOMMouseListener*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsXULKeyListenerImpl::Init(
  nsIDOMElement  * aElement,
  nsIDOMDocument * aDocument)
{
  element = aElement; // Weak reference. Don't addref it.

  nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(aDocument);
  if (!xulDoc)
    return NS_ERROR_FAILURE;

  mDOMDocument = xulDoc; // Weak reference.

  // Compiled-in defaults, in case we can't get LookAndFeel --
  // command for mac, control for all other platforms.
#ifdef XP_MAC
  mXULKeyModifier = nsIDOMKeyEvent::DOM_VK_META;
#else
  mXULKeyModifier = nsIDOMKeyEvent::DOM_VK_CONTROL;
#endif

  // Get the accelerator key value from prefs, overriding the default:
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && prefs)
  {
    rv = prefs->GetIntPref("ui.key.accelKey",
                           (PRInt32*)&mXULKeyModifier);
  }
#ifdef DEBUG_akkana
  else
  {
    NS_ASSERTION(PR_FALSE, "XULKeyListener couldn't get accel key from prefs!\n");
  }
#endif

  return NS_OK;
}

static PRBool PR_CALLBACK BreakScriptObjectCycle(nsHashKey *aKey, void *aData, void* closure)
{
  nsIDOMXULDocument* domdoc = dont_AddRef(NS_STATIC_CAST(nsIDOMXULDocument*, aData));
  nsCOMPtr<nsIDocument> document( do_QueryInterface(domdoc) );
  nsCOMPtr<nsIScriptGlobalObject> globalObject;
  document->GetScriptGlobalObject(getter_AddRefs(globalObject));
  nsCOMPtr<nsIScriptContext> context;
  if (globalObject) {
    // get the context for GC below
    globalObject->GetContext(getter_AddRefs(context));

    // break circular references
    globalObject->SetNewDocument(nsnull);
  }

  // break circular references  (could this be in the if block above?)
  document->SetScriptGlobalObject(nsnull);

  if (globalObject) {
    // break circular references
    // this must be after SetScriptGlobalObject(nsnull)
    // XXX Should this be SetDocShell instead?
    globalObject->SetDocShell(nsnull);
  }

  if (context) {
    // we must call GC on this context to clear newborn objects
    context->GC();
  }

  return PR_TRUE;
}

//static
void
nsXULKeyListenerImpl::Shutdown(void)
{
  if (mKeyBindingTable) {
    mKeyBindingTable->Enumerate(BreakScriptObjectCycle);
    delete mKeyBindingTable;
    mKeyBindingTable = nsnull;
  }
}

////////////////////////////////////////////////////////////////
// nsIDOMKeyListener

/**
 * Processes a key down event
 * @param aKeyEvent @see nsIDOMEvent.h
 * @returns whether the event was consumed or ignored. @see nsresult
 */
nsresult nsXULKeyListenerImpl::KeyDown(nsIDOMEvent* aKeyEvent)
{
  return DoKey(aKeyEvent, eKeyDown);
}

////////////////////////////////////////////////////////////////
/**
 * Processes a key release event
 * @param aKeyEvent @see nsIDOMEvent.h
 * @returns whether the event was consumed or ignored. @see nsresult
 */
nsresult nsXULKeyListenerImpl::KeyUp(nsIDOMEvent* aKeyEvent)
{
  return DoKey(aKeyEvent, eKeyUp);
}

////////////////////////////////////////////////////////////////
/**
 * Processes a key typed event
 * @param aKeyEvent @see nsIDOMEvent.h
 * @returns whether the event was consumed or ignored. @see nsresult
 *
 */
 //  // Get the main document
 //  // find the keyset
 // iterate over key(s) looking for appropriate handler
nsresult nsXULKeyListenerImpl::KeyPress(nsIDOMEvent* aKeyEvent)
{
  return DoKey(aKeyEvent, eKeyPress);
}

nsresult nsXULKeyListenerImpl::DoKey(nsIDOMEvent* aKeyEvent, eEventType aEventType)
{
  // Check the preventDefault flag.  We don't ever execute a XUL key binding
  // if this flag is set.
  nsCOMPtr<nsIDOMNSUIEvent> evt = do_QueryInterface(aKeyEvent);
  PRBool prevent;
  evt->GetPreventDefault(&prevent);
  if (prevent)
    return NS_OK;

  nsresult ret = NS_OK;

  if (!aKeyEvent)
    return ret;

  if (!mDOMDocument)
    return ret;

  // Get DOMEvent target
  nsCOMPtr<nsIDOMEventTarget> target = nsnull;
  aKeyEvent->GetTarget(getter_AddRefs(target));

  nsCOMPtr<nsPIDOMWindow> piWindow;

  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aKeyEvent);
  // Find a keyset node
  // Get the current focused object from the command dispatcher
  nsCOMPtr<nsIDOMXULCommandDispatcher> commandDispatcher;
  mDOMDocument->GetCommandDispatcher(getter_AddRefs(commandDispatcher));
 
  nsCOMPtr<nsIDOMWindowInternal> domWindow;
  commandDispatcher->GetFocusedWindow(getter_AddRefs(domWindow));
  piWindow = do_QueryInterface(domWindow);

  // Locate the key node and execute the JS on a match.
  PRBool handled = PR_FALSE;
  
  nsCAutoString browserFile("chrome://communicator/content/browserBindings.xul");
  nsCAutoString editorFile("chrome://communicator/content/editorBindings.xul");
  nsCAutoString browserPlatformFile("chrome://communicator/content/platformBrowserBindings.xul");
  nsCAutoString editorPlatformFile("chrome://communicator/content/platformEditorBindings.xul");

  nsresult result;

  if (!handled) {
    while (piWindow && !handled) {
      // See if we have a XUL document. Give it a crack.
      nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(piWindow);
      nsCOMPtr<nsIDOMDocument> windowDoc;
      domWindow->GetDocument(getter_AddRefs(windowDoc));
      nsCOMPtr<nsIDOMXULDocument> xulWindowDoc = do_QueryInterface(windowDoc);
      if (xulWindowDoc) {
        // Give the local key bindings in this XUL file a shot.
        LocateAndExecuteKeyBinding(keyEvent, aEventType, xulWindowDoc, handled);
      }

      if (!handled) {
        // Give the DOM window's associated key binding doc a shot.
        // XXX Check to see if we're in edit mode (how??!)
        nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(piWindow));
        nsCOMPtr<nsIDocShell> docShell;
        if(sgo)
          result = sgo->GetDocShell(getter_AddRefs(docShell));

        nsCOMPtr<nsIPresShell> presShell;
        if(docShell)
         docShell->GetPresShell(getter_AddRefs(presShell));

        PRBool editorHasBindings = PR_FALSE;
        nsCOMPtr<nsIDOMXULDocument> document;
        nsCOMPtr<nsIDOMXULDocument> platformDoc;
        if (presShell) {
          PRBool isEditor;
          if (NS_SUCCEEDED(presShell->GetDisplayNonTextSelection(&isEditor)) && isEditor) {
            editorHasBindings = PR_TRUE;
            GetKeyBindingDocument(editorPlatformFile, getter_AddRefs(platformDoc));
            GetKeyBindingDocument(editorFile, getter_AddRefs(document));
          }
        }
        if (!editorHasBindings) {
          GetKeyBindingDocument(browserPlatformFile, getter_AddRefs(platformDoc));
          GetKeyBindingDocument(browserFile, getter_AddRefs(document));
        }

        if (platformDoc)
          LocateAndExecuteKeyBinding(keyEvent, aEventType, platformDoc, handled);

        if (!handled && document)
          LocateAndExecuteKeyBinding(keyEvent, aEventType, document, handled);
      }

      // Move up to the parent DOM window. Need to use the private API
      // to cross sandboxes
      nsCOMPtr<nsPIDOMWindow> piParent;
      piWindow->GetPrivateParent(getter_AddRefs(piParent));
      piWindow = piParent;
    }
  }

  return ret;
}

PRBool nsXULKeyListenerImpl::IsMatchingKeyCode(const PRUint32 theChar, const nsString &keyName)
{
  PRBool ret = PR_FALSE;

  //printf("theChar = %d \n", theChar);
  //printf("keyName = %s \n", keyName.ToNewCString());
  //printf("\n");

  switch ( theChar ) {
    case VK_CANCEL:
      if(keyName == NS_LITERAL_STRING("VK_CANCEL"))
        ret = PR_TRUE;
        break;
    case VK_BACK:
      if(keyName == NS_LITERAL_STRING("VK_BACK"))
        ret = PR_TRUE;
        break;
    case VK_TAB:
      if(keyName == NS_LITERAL_STRING("VK_TAB"))
        ret = PR_TRUE;
        break;
    case VK_CLEAR:
      if(keyName == NS_LITERAL_STRING("VK_CLEAR"))
        ret = PR_TRUE;
        break;
    case VK_RETURN:
      if(keyName == NS_LITERAL_STRING("VK_RETURN"))
        ret = PR_TRUE;
        break;
    case VK_ENTER:
      if(keyName == NS_LITERAL_STRING("VK_ENTER"))
        ret = PR_TRUE;
        break;
    case VK_SHIFT:
      if(keyName == NS_LITERAL_STRING("VK_SHIFT"))
        ret = PR_TRUE;
        break;
    case VK_CONTROL:
      if(keyName == NS_LITERAL_STRING("VK_CONTROL"))
        ret = PR_TRUE;
        break;
    case VK_ALT:
      if(keyName == NS_LITERAL_STRING("VK_ALT"))
        ret = PR_TRUE;
        break;
    case VK_PAUSE:
      if(keyName == NS_LITERAL_STRING("VK_PAUSE"))
        ret = PR_TRUE;
        break;
    case VK_CAPS_LOCK:
      if(keyName == NS_LITERAL_STRING("VK_CAPS_LOCK"))
        ret = PR_TRUE;
        break;
    case VK_ESCAPE:
      if(keyName == NS_LITERAL_STRING("VK_ESCAPE"))
        ret = PR_TRUE;
        break;
    case VK_SPACE:
      if(keyName == NS_LITERAL_STRING("VK_SPACE"))
        ret = PR_TRUE;
        break;
    case VK_PAGE_UP:
      if(keyName == NS_LITERAL_STRING("VK_PAGE_UP"))
        ret = PR_TRUE;
        break;
    case VK_PAGE_DOWN:
      if(keyName == NS_LITERAL_STRING("VK_PAGE_DOWN"))
        ret = PR_TRUE;
        break;
    case VK_END:
      if(keyName == NS_LITERAL_STRING("VK_END"))
        ret = PR_TRUE;
        break;
    case VK_HOME:
      if(keyName == NS_LITERAL_STRING("VK_HOME"))
        ret = PR_TRUE;
        break;
    case VK_LEFT:
      if(keyName == NS_LITERAL_STRING("VK_LEFT"))
        ret = PR_TRUE;
        break;
    case VK_UP:
      if(keyName == NS_LITERAL_STRING("VK_UP"))
        ret = PR_TRUE;
        break;
    case VK_RIGHT:
      if(keyName == NS_LITERAL_STRING("VK_RIGHT"))
        ret = PR_TRUE;
        break;
    case VK_DOWN:
      if(keyName == NS_LITERAL_STRING("VK_DOWN"))
        ret = PR_TRUE;
        break;
    case VK_PRINTSCREEN:
      if(keyName == NS_LITERAL_STRING("VK_PRINTSCREEN"))
        ret = PR_TRUE;
        break;
    case VK_INSERT:
      if(keyName == NS_LITERAL_STRING("VK_INSERT"))
        ret = PR_TRUE;
        break;
    case VK_DELETE:
      if(keyName == NS_LITERAL_STRING("VK_DELETE"))
        ret = PR_TRUE;
        break;
    case VK_0:
      if(keyName == NS_LITERAL_STRING("VK_0"))
        ret = PR_TRUE;
        break;
    case VK_1:
      if(keyName == NS_LITERAL_STRING("VK_1"))
        ret = PR_TRUE;
        break;
    case VK_2:
      if(keyName == NS_LITERAL_STRING("VK_2"))
        ret = PR_TRUE;
        break;
    case VK_3:
      if(keyName == NS_LITERAL_STRING("VK_3"))
        ret = PR_TRUE;
        break;
    case VK_4:
      if(keyName == NS_LITERAL_STRING("VK_4"))
        ret = PR_TRUE;
        break;
    case VK_5:
      if(keyName == NS_LITERAL_STRING("VK_5"))
        ret = PR_TRUE;
        break;
    case VK_6:
      if(keyName == NS_LITERAL_STRING("VK_6"))
        ret = PR_TRUE;
        break;
    case VK_7:
      if(keyName == NS_LITERAL_STRING("VK_7"))
        ret = PR_TRUE;
        break;
    case VK_8:
      if(keyName == NS_LITERAL_STRING("VK_8"))
        ret = PR_TRUE;
        break;
    case VK_9:
      if(keyName == NS_LITERAL_STRING("VK_9"))
        ret = PR_TRUE;
        break;
    case VK_SEMICOLON:
      if(keyName == NS_LITERAL_STRING("VK_SEMICOLON"))
        ret = PR_TRUE;
        break;
    case VK_EQUALS:
      if(keyName == NS_LITERAL_STRING("VK_EQUALS"))
        ret = PR_TRUE;
        break;
    case VK_A:
      if(keyName == NS_LITERAL_STRING("VK_A")  || keyName == NS_LITERAL_STRING("A") || keyName == NS_LITERAL_STRING("a"))
        ret = PR_TRUE;
        break;
    case VK_B:
      if(keyName == NS_LITERAL_STRING("VK_B") || keyName == NS_LITERAL_STRING("B") || keyName == NS_LITERAL_STRING("b"))
        ret = PR_TRUE;
    break;
    case VK_C:
      if(keyName == NS_LITERAL_STRING("VK_C")  || keyName == NS_LITERAL_STRING("C") || keyName == NS_LITERAL_STRING("c"))
        ret = PR_TRUE;
        break;
    case VK_D:
      if(keyName == NS_LITERAL_STRING("VK_D")  || keyName == NS_LITERAL_STRING("D") || keyName == NS_LITERAL_STRING("d"))
        ret = PR_TRUE;
        break;
    case VK_E:
      if(keyName == NS_LITERAL_STRING("VK_E")  || keyName == NS_LITERAL_STRING("E") || keyName == NS_LITERAL_STRING("e"))
        ret = PR_TRUE;
        break;
    case VK_F:
      if(keyName == NS_LITERAL_STRING("VK_F")  || keyName == NS_LITERAL_STRING("F") || keyName == NS_LITERAL_STRING("f"))
        ret = PR_TRUE;
        break;
    case VK_G:
      if(keyName == NS_LITERAL_STRING("VK_G")  || keyName == NS_LITERAL_STRING("G") || keyName == NS_LITERAL_STRING("g"))
        ret = PR_TRUE;
        break;
    case VK_H:
      if(keyName == NS_LITERAL_STRING("VK_H")  || keyName == NS_LITERAL_STRING("H") || keyName == NS_LITERAL_STRING("h"))
        ret = PR_TRUE;
        break;
    case VK_I:
      if(keyName == NS_LITERAL_STRING("VK_I")  || keyName == NS_LITERAL_STRING("I") || keyName == NS_LITERAL_STRING("i"))
        ret = PR_TRUE;
        break;
    case VK_J:
      if(keyName == NS_LITERAL_STRING("VK_J")  || keyName == NS_LITERAL_STRING("J") || keyName == NS_LITERAL_STRING("j"))
        ret = PR_TRUE;
        break;
    case VK_K:
      if(keyName == NS_LITERAL_STRING("VK_K")  || keyName == NS_LITERAL_STRING("K") || keyName == NS_LITERAL_STRING("k"))
        ret = PR_TRUE;
        break;
    case VK_L:
      if(keyName == NS_LITERAL_STRING("VK_L")  || keyName == NS_LITERAL_STRING("L") || keyName == NS_LITERAL_STRING("l"))
        ret = PR_TRUE;
        break;
    case VK_M:
      if(keyName == NS_LITERAL_STRING("VK_M")  || keyName == NS_LITERAL_STRING("M") || keyName == NS_LITERAL_STRING("m"))
        ret = PR_TRUE;
        break;
    case VK_N:
      if(keyName == NS_LITERAL_STRING("VK_N")  || keyName == NS_LITERAL_STRING("N") || keyName == NS_LITERAL_STRING("n"))
        ret = PR_TRUE;
        break;
    case VK_O:
      if(keyName == NS_LITERAL_STRING("VK_O")  || keyName == NS_LITERAL_STRING("O") || keyName == NS_LITERAL_STRING("o"))
        ret = PR_TRUE;
        break;
    case VK_P:
      if(keyName == NS_LITERAL_STRING("VK_P")  || keyName == NS_LITERAL_STRING("P") || keyName == NS_LITERAL_STRING("p"))
        ret = PR_TRUE;
        break;
    case VK_Q:
      if(keyName == NS_LITERAL_STRING("VK_Q")  || keyName == NS_LITERAL_STRING("Q") || keyName == NS_LITERAL_STRING("q"))
        ret = PR_TRUE;
        break;
    case VK_R:
      if(keyName == NS_LITERAL_STRING("VK_R")  || keyName == NS_LITERAL_STRING("R") || keyName == NS_LITERAL_STRING("r"))
        ret = PR_TRUE;
        break;
    case VK_S:
      if(keyName == NS_LITERAL_STRING("VK_S")  || keyName == NS_LITERAL_STRING("S") || keyName == NS_LITERAL_STRING("s"))
        ret = PR_TRUE;
        break;
    case VK_T:
      if(keyName == NS_LITERAL_STRING("VK_T")  || keyName == NS_LITERAL_STRING("T") || keyName == NS_LITERAL_STRING("t"))
        ret = PR_TRUE;
        break;
    case VK_U:
      if(keyName == NS_LITERAL_STRING("VK_U")  || keyName == NS_LITERAL_STRING("U") || keyName == NS_LITERAL_STRING("u"))
        ret = PR_TRUE;
        break;
    case VK_V:
      if(keyName == NS_LITERAL_STRING("VK_V")  || keyName == NS_LITERAL_STRING("V") || keyName == NS_LITERAL_STRING("v"))
        ret = PR_TRUE;
        break;
    case VK_W:
      if(keyName == NS_LITERAL_STRING("VK_W")  || keyName == NS_LITERAL_STRING("W") || keyName == NS_LITERAL_STRING("w"))
        ret = PR_TRUE;
        break;
    case VK_X:
      if(keyName == NS_LITERAL_STRING("VK_X")  || keyName == NS_LITERAL_STRING("X") || keyName == NS_LITERAL_STRING("x"))
        ret = PR_TRUE;
        break;
    case VK_Y:
      if(keyName == NS_LITERAL_STRING("VK_Y")  || keyName == NS_LITERAL_STRING("Y") || keyName == NS_LITERAL_STRING("y"))
        ret = PR_TRUE;
        break;
    case VK_Z:
      if(keyName == NS_LITERAL_STRING("VK_Z")  || keyName == NS_LITERAL_STRING("Z") || keyName == NS_LITERAL_STRING("z"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD0:
      if(keyName == NS_LITERAL_STRING("VK_NUMPAD0"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD1:
      if(keyName == NS_LITERAL_STRING("VK_NUMPAD1"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD2:
      if(keyName == NS_LITERAL_STRING("VK_NUMPAD2"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD3:
      if(keyName == NS_LITERAL_STRING("VK_NUMPAD3"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD4:
      if(keyName == NS_LITERAL_STRING("VK_NUMPAD4"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD5:
      if(keyName == NS_LITERAL_STRING("VK_NUMPAD5"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD6:
      if(keyName == NS_LITERAL_STRING("VK_NUMPAD6"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD7:
      if(keyName == NS_LITERAL_STRING("VK_NUMPAD7"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD8:
      if(keyName == NS_LITERAL_STRING("VK_NUMPAD8"))
        ret = PR_TRUE;
        break;
    case VK_NUMPAD9:
      if(keyName == NS_LITERAL_STRING("VK_NUMPAD9"))
        ret = PR_TRUE;
        break;
    case VK_MULTIPLY:
      if(keyName == NS_LITERAL_STRING("VK_MULTIPLY"))
        ret = PR_TRUE;
        break;
    case VK_ADD:
      if(keyName == NS_LITERAL_STRING("VK_ADD"))
        ret = PR_TRUE;
        break;
    case VK_SEPARATOR:
      if(keyName == NS_LITERAL_STRING("VK_SEPARATOR"))
        ret = PR_TRUE;
        break;
    case VK_SUBTRACT:
      if(keyName == NS_LITERAL_STRING("VK_SUBTRACT"))
        ret = PR_TRUE;
        break;
    case VK_DECIMAL:
      if(keyName == NS_LITERAL_STRING("VK_DECIMAL"))
        ret = PR_TRUE;
        break;
    case VK_DIVIDE:
      if(keyName == NS_LITERAL_STRING("VK_DIVIDE"))
        ret = PR_TRUE;
        break;
    case VK_F1:
      if(keyName == NS_LITERAL_STRING("VK_F1"))
        ret = PR_TRUE;
        break;
    case VK_F2:
      if(keyName == NS_LITERAL_STRING("VK_F2"))
        ret = PR_TRUE;
        break;
    case VK_F3:
      if(keyName == NS_LITERAL_STRING("VK_F3"))
        ret = PR_TRUE;
        break;
    case VK_F4:
      if(keyName == NS_LITERAL_STRING("VK_F4"))
        ret = PR_TRUE;
        break;
    case VK_F5:
      if(keyName == NS_LITERAL_STRING("VK_F5"))
        ret = PR_TRUE;
        break;
    case VK_F6:
      if(keyName == NS_LITERAL_STRING("VK_F6"))
        ret = PR_TRUE;
        break;
    case VK_F7:
      if(keyName == NS_LITERAL_STRING("VK_F7"))
        ret = PR_TRUE;
        break;
    case VK_F8:
      if(keyName == NS_LITERAL_STRING("VK_F8"))
        ret = PR_TRUE;
        break;
    case VK_F9:
      if(keyName == NS_LITERAL_STRING("VK_F9"))
        ret = PR_TRUE;
        break;
    case VK_F10:
      if(keyName == NS_LITERAL_STRING("VK_F10"))
        ret = PR_TRUE;
        break;
    case VK_F11:
      if(keyName == NS_LITERAL_STRING("VK_F11"))
        ret = PR_TRUE;
        break;
    case VK_F12:
      if(keyName == NS_LITERAL_STRING("VK_F12"))
        ret = PR_TRUE;
        break;
    case VK_F13:
      if(keyName == NS_LITERAL_STRING("VK_F13"))
        ret = PR_TRUE;
        break;
    case VK_F14:
      if(keyName == NS_LITERAL_STRING("VK_F14"))
        ret = PR_TRUE;
        break;
    case VK_F15:
      if(keyName == NS_LITERAL_STRING("VK_F15"))
        ret = PR_TRUE;
        break;
    case VK_F16:
      if(keyName == NS_LITERAL_STRING("VK_F16"))
        ret = PR_TRUE;
        break;
    case VK_F17:
      if(keyName == NS_LITERAL_STRING("VK_F17"))
        ret = PR_TRUE;
        break;
    case VK_F18:
      if(keyName == NS_LITERAL_STRING("VK_F18"))
        ret = PR_TRUE;
        break;
    case VK_F19:
      if(keyName == NS_LITERAL_STRING("VK_F19"))
        ret = PR_TRUE;
        break;
    case VK_F20:
      if(keyName == NS_LITERAL_STRING("VK_F20"))
        ret = PR_TRUE;
        break;
    case VK_F21:
      if(keyName == NS_LITERAL_STRING("VK_F21"))
        ret = PR_TRUE;
        break;
    case VK_F22:
      if(keyName == NS_LITERAL_STRING("VK_F22"))
        ret = PR_TRUE;
        break;
    case VK_F23:
      if(keyName == NS_LITERAL_STRING("VK_F23"))
        ret = PR_TRUE;
        break;
    case VK_F24:
      if(keyName == NS_LITERAL_STRING("VK_F24"))
        ret = PR_TRUE;
        break;
    case VK_NUM_LOCK:
      if(keyName == NS_LITERAL_STRING("VK_NUM_LOCK"))
        ret = PR_TRUE;
        break;
    case VK_SCROLL_LOCK:
      if(keyName == NS_LITERAL_STRING("VK_SCROLL_LOCK"))
        ret = PR_TRUE;
        break;
    case VK_COMMA:
      if(keyName == NS_LITERAL_STRING("VK_COMMA"))
        ret = PR_TRUE;
        break;
    case VK_PERIOD:
      if(keyName == NS_LITERAL_STRING("VK_PERIOD"))
        ret = PR_TRUE;
        break;
    case VK_SLASH:
      if(keyName == NS_LITERAL_STRING("VK_SLASH"))
        ret = PR_TRUE;
        break;
    case VK_BACK_QUOTE:
      if(keyName == NS_LITERAL_STRING("VK_BACK_QUOTE"))
        ret = PR_TRUE;
        break;
    case VK_OPEN_BRACKET:
      if(keyName == NS_LITERAL_STRING("VK_OPEN_BRACKET"))
        ret = PR_TRUE;
        break;
    case VK_BACK_SLASH:
      if(keyName == NS_LITERAL_STRING("VK_BACK_SLASH"))
        ret = PR_TRUE;
        break;
    case VK_CLOSE_BRACKET:
      if(keyName == NS_LITERAL_STRING("VK_CLOSE_BRACKET"))
        ret = PR_TRUE;
        break;
    case VK_QUOTE:
      if(keyName == NS_LITERAL_STRING("VK_QUOTE"))
        ret = PR_TRUE;
        break;
  }

  return ret;
}

PRBool nsXULKeyListenerImpl::IsMatchingCharCode(const nsString &theChar, const nsString &keyName)
{
  PRBool ret = PR_FALSE;

  //printf("theChar = %s \n", theChar.ToNewCString());
  //printf("keyName = %s \n", keyName.ToNewCString());
  //printf("\n");
  if(theChar == keyName)
    ret = PR_TRUE;

  return ret;
}

NS_IMETHODIMP nsXULKeyListenerImpl::GetKeyBindingDocument(nsCAutoString& aURLStr, nsIDOMXULDocument** aResult)
{
  nsCOMPtr<nsIDOMXULDocument> document;
  // This could only happen after shutdown.  Is this right??
  NS_ASSERTION(mKeyBindingTable, "Doing stuff after module shutdown!");
  if (!mKeyBindingTable) return NS_ERROR_NULL_POINTER;
  if (!aURLStr.IsEmpty()) {
    nsCOMPtr<nsIURL> uri;
    nsComponentManager::CreateInstance("@mozilla.org/network/standard-url;1",
                                          nsnull,
                                          NS_GET_IID(nsIURL),
                                          getter_AddRefs(uri));
    uri->SetSpec(aURLStr);

    // We've got a file.  Check our key binding file cache.
    nsIURIKey key(uri);
    document = dont_AddRef(NS_STATIC_CAST(nsIDOMXULDocument*, mKeyBindingTable->Get(&key)));

    if (!document) {
      LoadKeyBindingDocument(uri, getter_AddRefs(document));
      if (document) {
        // Put the key binding doc into our table.
        mKeyBindingTable->Put(&key, document);
      }
    }
  }

  *aResult = document;
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP nsXULKeyListenerImpl::LoadKeyBindingDocument(nsIURI* aURI, nsIDOMXULDocument** aResult)
{
  *aResult = nsnull;

  // Create the XUL document
  nsCOMPtr<nsIDOMXULDocument> doc;
  nsresult rv = nsComponentManager::CreateInstance(kXULDocumentCID, nsnull,
                                                   NS_GET_IID(nsIDOMXULDocument),
                                                   getter_AddRefs(doc));

  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIXULDocument> xulDoc = do_QueryInterface(doc, &rv);
  if (NS_FAILED(rv)) return rv;

  xulDoc->SetIsKeybindingDocument(PR_TRUE);

  // Now we have to synchronously load the key binding file.
  // Create a XUL content sink, a parser, and kick off a load for
  // the overlay.

  nsCOMPtr<nsIChannel> channel;
  rv = NS_OpenURI(getter_AddRefs(channel), aURI, nsnull);
  if (NS_FAILED(rv)) return rv;

  // Create a new prototype document
  nsCOMPtr<nsIXULPrototypeDocument> proto;
  rv = NS_NewXULPrototypeDocument(nsnull, NS_GET_IID(nsIXULPrototypeDocument), getter_AddRefs(proto));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsISupports> owner;
  rv = channel->GetOwner(getter_AddRefs(owner));
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(owner);
  proto->SetDocumentPrincipal(principal);

  // Set master and current prototype
  xulDoc->SetMasterPrototype(proto);
  xulDoc->SetCurrentPrototype(proto);


  rv = proto->SetURI(aURI);
  if (NS_FAILED(rv)) return rv;

  xulDoc->SetDocumentURL(aURI);
  xulDoc->PrepareStyleSheets(aURI);

  nsCOMPtr<nsIXULContentSink> sink;
  rv = nsComponentManager::CreateInstance(kXULContentSinkCID,
                                          nsnull,
                                          NS_GET_IID(nsIXULContentSink),
                                          getter_AddRefs(sink));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create XUL content sink");
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDocument> document = do_QueryInterface(doc);
  rv = sink->Init(document, proto);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to initialize content sink");
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIParser> parser;
  rv = nsComponentManager::CreateInstance(kParserCID,
                                          nsnull,
                                          NS_GET_IID(nsIParser),
                                          getter_AddRefs(parser));
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create parser");
  if (NS_FAILED(rv)) return rv;

  parser->SetCommand(eViewNormal);

  nsAutoString charset(NS_LITERAL_STRING("UTF-8"));
  parser->SetDocumentCharset(charset, kCharsetFromDocTypeDefault);
  parser->SetContentSink(sink); // grabs a reference to the parser

  // Now do a blocking synchronous parse of the file.
  nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(parser, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = parser->Parse(aURI);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIInputStream> in;
  PRUint32 sourceOffset = 0;
  rv = channel->OpenInputStream(getter_AddRefs(in));

  // If we couldn't open the channel, then just return.
  if (NS_FAILED(rv)) return NS_OK;

  NS_ASSERTION(in != nsnull, "no input stream");
  if (! in) return NS_ERROR_FAILURE;

  rv = NS_ERROR_OUT_OF_MEMORY;
  nsProxyStream* proxy = new nsProxyStream();
  if (! proxy)
    return NS_ERROR_FAILURE;

  listener->OnStartRequest(channel, nsnull);
  while (PR_TRUE) {
    char buf[1024];
    PRUint32 readCount;

    if (NS_FAILED(rv = in->Read(buf, sizeof(buf), &readCount)))
        break; // error

    if (readCount == 0)
        break; // eof

    proxy->SetBuffer(buf, readCount);

    rv = listener->OnDataAvailable(channel, nsnull, proxy, sourceOffset, readCount);
    sourceOffset += readCount;
    if (NS_FAILED(rv))
        break;
  }
  listener->OnStopRequest(channel, nsnull, NS_OK, nsnull);

  // don't leak proxy!
  proxy->Close();
  delete proxy;

  // The document is parsed. We now have a prototype document.
  // Everything worked, so we can just hand this back now.
  *aResult = doc;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXULKeyListenerImpl::LocateAndExecuteKeyBinding(nsIDOMKeyEvent* aEvent, eEventType aEventType, nsIDOMXULDocument* aDocument,
                                             PRBool& aHandledFlag)
{
  aHandledFlag = PR_FALSE;

  // locate the window element which holds the top level key bindings
  nsCOMPtr<nsIDOMElement> rootElement;
  aDocument->GetDocumentElement(getter_AddRefs(rootElement));
  if (!rootElement)
    return NS_OK;

  //nsAutoString rootName;
  //rootElement->GetNodeName(rootName);
  //printf("Root Node [%s] \n", rootName.ToNewCString()); // this leaks

  nsCOMPtr<nsIDOMNode> currNode;
  rootElement->GetFirstChild(getter_AddRefs(currNode));

  while (currNode) {
    nsAutoString currNodeType;
    nsCOMPtr<nsIDOMElement> currElement(do_QueryInterface(currNode));
    if (currElement) {
      currElement->GetNodeName(currNodeType);
      if (currNodeType == NS_LITERAL_STRING("keyset"))
        return HandleEventUsingKeyset(currElement, aEvent, aEventType, aDocument, aHandledFlag);
    }

    nsCOMPtr<nsIDOMNode> prevNode(currNode);
    prevNode->GetNextSibling(getter_AddRefs(currNode));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULKeyListenerImpl::HandleEventUsingKeyset(nsIDOMElement* aKeysetElement, nsIDOMKeyEvent* aKeyEvent, eEventType aEventType,
                                         nsIDOMXULDocument* aDocument, PRBool& aHandledFlag)
{

  nsAutoString trueString; trueString.AssignWithConversion("true");
  nsAutoString falseString; falseString.AssignWithConversion("false");

#undef DEBUG_XUL_KEYS
#ifdef DEBUG_XUL_KEYS
  if (aEventType == eKeyPress)
  {
      PRUint32 charcode, keycode;
    keyEvent->GetCharCode(&charcode);
    keyEvent->GetKeyCode(&keycode);
    printf("DoKey [%s]: key code 0x%d, char code '%c', ",
           (aEventType == eKeyPress ? "KeyPress" : ""), keycode, charcode);
    PRBool ismod;
    keyEvent->GetShiftKey(&ismod);
    if (ismod) printf("[Shift] ");
    keyEvent->GetCtrlKey(&ismod);
    if (ismod) printf("[Ctrl] ");
    keyEvent->GetAltKey(&ismod);
    if (ismod) printf("[Alt] ");
    keyEvent->GetMetaKey(&ismod);
    if (ismod) printf("[Meta] ");
    printf("\n");
  }
#endif /* DEBUG_XUL_KEYS */

  // Given the DOM node and Key Event
  // Walk the node's children looking for 'key' types

  // XXX Use the key-equivalent CSS3 property to obtain the
  // appropriate modifier for this keyset.
  // TODO. For now it's hardcoded.

  // If the node isn't tagged disabled
  // Compares the received key code to found 'key' types
  // Executes command if found
  // Marks event as consumed

  nsCOMPtr<nsIDOMNode> keyNode;
  aKeysetElement->GetFirstChild(getter_AddRefs(keyNode));
  while (keyNode) {
    nsCOMPtr<nsIDOMElement> keyElement(do_QueryInterface(keyNode));
    if (!keyElement)
      continue;

    nsAutoString property;
    keyElement->GetNodeName(property);
    //printf("keyNodeType [%s] \n", keyNodeType.ToNewCString()); // this leaks

    if (property == NS_LITERAL_STRING("key")) {
      //printf("onkeypress [%s] \n", cmdToExecute.ToNewCString()); // this leaks
      do {
        property = falseString;
        keyElement->GetAttribute(NS_LITERAL_STRING("disabled"), property);
        if (property == trueString) {
          break;
        }

        PRUint32 theChar;
        
        nsAutoString keyName; // This should be phased out for keycode and charcode
        keyElement->GetAttribute(NS_LITERAL_STRING("key"), keyName);
        if ( !keyName.IsEmpty() ) {
            if (aEventType != eKeyPress)
                break;
            aKeyEvent->GetCharCode(&theChar);
        }
        //printf("Found key [%s] \n", keyName.ToNewCString()); // this leaks
        PRBool   gotCharCode = PR_FALSE;
        PRBool   gotKeyCode  = PR_FALSE;
        
        if ( keyName.IsEmpty() )
        {
	        keyElement->GetAttribute(NS_LITERAL_STRING("charcode"), keyName);
	        if(keyName.IsEmpty()) {
	          keyElement->GetAttribute(NS_LITERAL_STRING("keycode"), keyName);
	          if(keyName.IsEmpty()) {
	            // HACK for temporary compatibility
	            if(aEventType == eKeyPress)
	              aKeyEvent->GetCharCode(&theChar);
	            else
	              aKeyEvent->GetKeyCode(&theChar);

	          } else {
	            // We want a keycode
	            aKeyEvent->GetKeyCode(&theChar);
	            gotKeyCode = PR_TRUE;
	          }
	        } else {
	          // We want a charcode
	          aKeyEvent->GetCharCode(&theChar);
	          gotCharCode = PR_TRUE;
	        }
        }

		if(keyName.IsEmpty())
			break;

        PRUnichar tempChar[2];
        tempChar[0] = theChar;
        tempChar[1] = 0;
        nsAutoString tempChar2(tempChar);
        //printf("compare key [%s] \n", tempChar2.ToNewCString()); // this leaks
        // NOTE - convert theChar and keyName to upper
        keyName.ToUpperCase();
        tempChar2.ToUpperCase();

        PRBool isMatching;
        if(gotCharCode){
            isMatching = IsMatchingCharCode(tempChar2, keyName);
        } else if(gotKeyCode){
          isMatching = IsMatchingKeyCode(theChar, keyName);
        }

        // HACK for backward compatibility
        if(!gotCharCode && ! gotKeyCode){
          isMatching = IsMatchingCharCode(tempChar2, keyName);
        }

        if (!isMatching) {
          break;
        }

        // This is gross -- we're doing string compares
        // every time we loop over this list!

        // Modifiers in XUL files are tri-state --
        //   true, false, and unspecified.
        // If a modifier is unspecified, we don't check
        // the status of that modifier (always match).

        // Get the attribute for the "xulkey" modifier.
        nsAutoString xproperty;
        keyElement->GetAttribute(NS_LITERAL_STRING("xulkey"),
                                 xproperty);

        // Is the modifier key set in the event?
        PRBool isModKey = PR_FALSE;

                  // Check whether the shift key fails to match:
        aKeyEvent->GetShiftKey(&isModKey);
        property.SetLength(0);
        keyElement->GetAttribute(NS_LITERAL_STRING("shift"), property);
        if ((property == trueString && !isModKey)
            || (property == falseString && isModKey))
            break;
        // and also the xul key, if it's specified to be shift:
        if (nsIDOMKeyEvent::DOM_VK_SHIFT == mXULKeyModifier &&
            ((xproperty == trueString && !isModKey)
             || (xproperty == falseString && isModKey)))
            break;

        // and the control key:
        aKeyEvent->GetCtrlKey(&isModKey);
            property.SetLength(0);
        keyElement->GetAttribute(NS_LITERAL_STRING("control"), property);
        if ((property == trueString && !isModKey)
          || (property == falseString && isModKey))
          break;
        // and if xul is control:
        if (nsIDOMKeyEvent::DOM_VK_CONTROL == mXULKeyModifier &&
            ((xproperty == trueString && !isModKey)
             || (xproperty == falseString && isModKey)))
            break;

        // and the alt key
        aKeyEvent->GetAltKey(&isModKey);
        property.SetLength(0);
        keyElement->GetAttribute(NS_LITERAL_STRING("alt"), property);
        if ((property == trueString && !isModKey)
          || (property == falseString && isModKey))
          break;
        // and if xul is alt:
        if (nsIDOMKeyEvent::DOM_VK_ALT == mXULKeyModifier &&
          ((xproperty == trueString && !isModKey)
           || (xproperty == falseString && isModKey)))
          break;

        // and the meta key
        aKeyEvent->GetMetaKey(&isModKey);
        property.SetLength(0);
        keyElement->GetAttribute(NS_LITERAL_STRING("meta"), property);
        if ((property == trueString && !isModKey)
          || (property == falseString && isModKey))
          break;
        // and if xul is meta:
        if (nsIDOMKeyEvent::DOM_VK_META == mXULKeyModifier &&
          ((xproperty == trueString && !isModKey)
           || (xproperty == falseString && isModKey)))
          break;

        // We know we're handling this.
        aHandledFlag = PR_TRUE;

        // Get the cancel attribute.
        nsAutoString cancelValue;
        keyElement->GetAttribute(NS_LITERAL_STRING("cancel"), cancelValue);
        if (cancelValue == NS_LITERAL_STRING("true")) {
          return NS_OK;
        }

        // Modifier tests passed so execute onclick command
        nsAutoString cmdToExecute;
        nsAutoString oncommand;
        switch(aEventType) {
          case eKeyPress:
            keyElement->GetAttribute(NS_LITERAL_STRING("onkeypress"), cmdToExecute);
#if defined(DEBUG_saari)
            printf("onkeypress = %s\n",
                               cmdToExecute.ToNewCString());
#endif

            keyElement->GetAttribute(NS_LITERAL_STRING("oncommand"), oncommand);
#if defined(DEBUG_saari)
            printf("oncommand = %s\n", oncommand.ToNewCString());
#endif
          break;
          case eKeyDown:
            keyElement->GetAttribute(NS_LITERAL_STRING("onkeydown"), cmdToExecute);
          break;
          case eKeyUp:
            keyElement->GetAttribute(NS_LITERAL_STRING("onkeyup"), cmdToExecute);
          break;
        }

        // This code executes in every presentation context in which this
        // document is appearing.
        nsCOMPtr<nsIDocument> document = do_QueryInterface(aDocument);
        nsCOMPtr<nsIContent> content = do_QueryInterface(keyElement);
        /*if (aDocument != mDOMDocument) */{
          nsCOMPtr<nsIScriptEventHandlerOwner> handlerOwner = do_QueryInterface(keyElement);
          if (handlerOwner) {
            nsAutoString eventStr;
            switch(aEventType) {
              case eKeyPress:
                eventStr.AssignWithConversion("onkeypress");
                break;
              case eKeyDown:
                eventStr.AssignWithConversion("onkeydown");
                break;
              case eKeyUp:
                eventStr.AssignWithConversion("onkeyup");
                break;
            }

            // Look for a compiled event handler on the key element itself.
            nsCOMPtr<nsIAtom> eventName = getter_AddRefs(NS_NewAtom(eventStr));
            void* handler = nsnull;
            handlerOwner->GetCompiledEventHandler(eventName, &handler);

            nsCOMPtr<nsIScriptGlobalObject> masterGlobalObject;
            nsCOMPtr<nsIDocument> masterDoc = do_QueryInterface(mDOMDocument);
            masterDoc->GetScriptGlobalObject(getter_AddRefs(masterGlobalObject));
            nsCOMPtr<nsIScriptContext> masterContext;
            masterGlobalObject->GetContext(getter_AddRefs(masterContext));

            if (!handler) {
              // It hasn't been compiled before.
              nsCOMPtr<nsIScriptGlobalObject> globalObject;
              document->GetScriptGlobalObject(getter_AddRefs(globalObject));
              if (!globalObject) {
                NS_NewScriptGlobalObject(getter_AddRefs(globalObject));
                document->SetScriptGlobalObject(globalObject);
              }

              nsCOMPtr<nsIScriptContext> context;
              globalObject->GetContext(getter_AddRefs(context));
              if (!context) {
                NS_CreateScriptContext(globalObject, getter_AddRefs(context));
                globalObject->SetNewDocument(aDocument);
              }

              nsCOMPtr<nsIScriptObjectOwner> owner = do_QueryInterface(keyElement);
              void* scriptObject;
              owner->GetScriptObject(context, &scriptObject);

              nsCOMPtr<nsIContent> keyContent = do_QueryInterface(keyElement);
              nsAutoString value;
              keyContent->GetAttribute(kNameSpaceID_None, eventName, value);
              if (!value.IsEmpty()) {
                  if (handlerOwner) {
                      handlerOwner->CompileEventHandler(context, scriptObject, eventName, value, &handler);
                  }
                  else {
                      context->CompileEventHandler(scriptObject, eventName, value,
                                                   PR_TRUE, &handler);
                  }
              }
            }

            if (handler) {
              if(aDocument != mDOMDocument){
                nsCOMPtr<nsIDOMElement> rootElement;
                mDOMDocument->GetDocumentElement(getter_AddRefs(rootElement));
                nsCOMPtr<nsIScriptObjectOwner> owner = do_QueryInterface(rootElement);
                void* scriptObject;
                owner->GetScriptObject(masterContext, &scriptObject);

                masterContext->BindCompiledEventHandler(scriptObject, eventName, handler);

                nsCOMPtr<nsIDOMEventListener> eventListener;
                NS_NewJSEventListener(getter_AddRefs(eventListener), masterContext, owner);
                eventListener->HandleEvent(aKeyEvent);

                masterContext->BindCompiledEventHandler(scriptObject, eventName, nsnull);

                // Next three lines cause bug 28396
                //aKeyEvent->PreventBubble();
                //aKeyEvent->PreventCapture();
                //aKeyEvent->PreventDefault();
        
                return NS_OK;
              } else {
                nsCOMPtr<nsIScriptObjectOwner> owner = do_QueryInterface(keyElement);
                nsCOMPtr<nsIDOMEventListener> eventListener;
                NS_NewJSEventListener(getter_AddRefs(eventListener), masterContext, owner);
                eventListener->HandleEvent(aKeyEvent);
              }
            }
          }
        }

        aKeyEvent->PreventBubble();
        aKeyEvent->PreventCapture();
        aKeyEvent->PreventDefault();
          
        PRInt32 count = document->GetNumberOfShells();
        for (PRInt32 i = 0; i < count; i++) {
          nsCOMPtr<nsIPresContext> aPresContext;
          { /* scope the shell variable (prevents the PresShell
               from outliving its ViewManager if the key event
               happens to delete the window.) */
            // Retrieve the context in which our DOM event will fire.
            nsCOMPtr<nsIPresShell> shell = getter_AddRefs(document->GetShellAt(i));
            shell->GetPresContext(getter_AddRefs(aPresContext));
          }

          nsresult ret = NS_ERROR_BASE;

          if (aEventType == eKeyPress) {
            // Also execute the oncommand handler on a key press.
            // Execute the oncommand event handler.
            nsEventStatus stat = nsEventStatus_eIgnore;
            nsMouseEvent evt;
            evt.eventStructType = NS_EVENT;
            evt.message = NS_MENU_ACTION;
            content->HandleDOMEvent(aPresContext, &evt, nsnull, NS_EVENT_FLAG_INIT, &stat);
          }
          // Ok, we got this far and handled the event, so don't continue scanning nodes
          //printf("Keybind executed \n");
          return ret;
        } // end for (PRInt32 i = 0; i < count; i++)
      } while (PR_FALSE); // do { ...
    } // end if (keyNodeType.Equals("key"))

    nsCOMPtr<nsIDOMNode> oldkeyNode(keyNode);
    oldkeyNode->GetNextSibling(getter_AddRefs(keyNode));
  } // end while(keynode)

  return NS_OK;
}

////////////////////////////////////////////////////////////////
nsresult
NS_NewXULKeyListener(nsIXULKeyListener** aListener)
{
  nsXULKeyListenerImpl * listener = new nsXULKeyListenerImpl();
  if (!listener)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(listener);
  *aListener = listener;
  return NS_OK;
}
