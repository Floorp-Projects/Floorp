#include "GtkMozillaContainer.h"

#include "nsRepository.h"
#include "nsIWebShell.h"
#include "nsFileSpec.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kIWebShellContainerIID, NS_IWEB_SHELL_CONTAINER_IID);

GtkMozillaContainer::GtkMozillaContainer(GtkMozilla *moz)
{
  mWebShell = nsnull;
  width = height = 0;
  
  mozilla = moz;
  gtk_widget_set_app_paintable(GTK_WIDGET(moz), PR_TRUE);
}


GtkMozillaContainer::~GtkMozillaContainer(void)
{
  NS_IF_RELEASE(mWebShell);
}

void
GtkMozillaContainer::Show()
{
  GtkAllocation *alloc = &GTK_WIDGET(mozilla)->allocation;
    
  nsresult rv = nsRepository::CreateInstance(kWebShellCID, nsnull,
					     kIWebShellIID,
					     (void**)&mWebShell);

  if (NS_FAILED(rv) || !mWebShell) {
    printf("Cannot create WebShell!");
    return;
  }
  
  if (mozilla) {
    width = alloc->width;
    height = alloc->height;

    //printf("Init, size: %d, %d\n", width, height);
    mWebShell->Init((nsNativeWidget *)mozilla,
		    0, 0,
		    width, height);
    
    mWebShell->SetContainer(this);
  }
  if (mWebShell) {
    mWebShell->Show();
  }
}

void
GtkMozillaContainer::Resize(gint w, gint h)
{
  int new_size;
  //  printf("GtkMozillaContainer::Resize called width: %d, %d\n", w, h);
  new_size = ((width != w) || (height != h));
  if (new_size && mWebShell) {
    width = w;
    height = h;
    //printf("GtkMozillaContainer::Resize setting to: %d, %d\n", width, height);
    gtk_layout_set_size(GTK_LAYOUT(mozilla), width, height);
    mWebShell->SetBounds(0, 0, width, height);
  }
}

void
GtkMozillaContainer::LoadURL(const gchar *url)
{
  PRUnichar *u_url;
  int len, i;
  len = strlen(url);
  
  u_url = new (PRUnichar)[len+1];
  for (i=0;i<len;i++)
    u_url[i] = (PRUnichar) url[i];
  u_url[len] = 0;
  mWebShell->LoadURL(u_url);
  delete [] u_url;
}

void
GtkMozillaContainer::Stop()
{
  mWebShell->Stop();
}

void
GtkMozillaContainer::Reload(GtkMozillaReloadType type)
{
 mWebShell->Reload((nsURLReloadType)type);
}

gint
GtkMozillaContainer::Back()
{
  return NS_SUCCEEDED(mWebShell->Back());
}

gint
GtkMozillaContainer::CanBack()
{
 return mWebShell->CanBack()==NS_OK;
}

gint
GtkMozillaContainer::Forward()
{
 return NS_SUCCEEDED(mWebShell->Forward());
}

gint
GtkMozillaContainer::CanForward()
{
  nsresult rv = mWebShell->CanForward();
  return mWebShell->CanForward()==NS_OK;
}

gint
GtkMozillaContainer::GoTo(gint history_index)
{
  return NS_SUCCEEDED(mWebShell->GoTo((PRInt32)history_index));
}

gint
GtkMozillaContainer::GetHistoryLength()
{
  PRInt32 Result;
  
  if (NS_SUCCEEDED(mWebShell->GetHistoryLength(Result)))
    return (gint) Result;
  else
    return 0;
}

gint
GtkMozillaContainer::GetHistoryIndex()
{
  PRInt32 Result;
  
  if (NS_SUCCEEDED(mWebShell->GetHistoryIndex(Result)))
    return (gint) Result;
  else
    return 0;
}


NS_IMETHODIMP
GtkMozillaContainer::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsISupports *ifp = nsnull;

  if (aIID.Equals(kIWebShellContainerIID)) {
    ifp = (nsIWebShellContainer*)this;
  } else if(aIID.Equals(kISupportsIID)) {
    ifp = this;
  } else {
    *aInstancePtr = 0;
   
    return NS_NOINTERFACE;
  }

  *aInstancePtr = (void *)ifp;

  NS_ADDREF(ifp);
  
  return NS_OK;
}


nsrefcnt
GtkMozillaContainer::AddRef()
{
  // This object is not reference counted so we just return a number > 0
  return 3;
}


nsrefcnt
GtkMozillaContainer::Release()
{
  // This object is not reference counted so we just return a number > 0
  return 2;
}

static char *simple_unicode_to_char(const PRUnichar* aURL)
{
  int i;
  int len=0;
  const PRUnichar* ptr;
  char *str;

  ptr=aURL;
  while (*ptr++)
    len++;

  str = (char *)malloc(len+1);
  if (str==NULL)
    return NULL;
  
  for (i=0;i<len;i++) {
    if (aURL[i]<127)
      str[i] = (char) aURL[i]&0xff;
    else
      str[i] = '_';
  }
  str[len] = 0;
  return str;
}

NS_IMETHODIMP
GtkMozillaContainer::WillLoadURL(nsIWebShell* aShell,
				 const PRUnichar* aURL,
				 nsLoadType aReason)
{
  gint result = 1;
  char *url = simple_unicode_to_char(aURL);
  gtk_signal_emit_by_name(GTK_OBJECT(mozilla), "will_load_url",
			  url, (gint) aReason, &result);
  if (url)
    free(url);
  
  return (result)?NS_OK:NS_ERROR_FAILURE;
}


NS_IMETHODIMP
GtkMozillaContainer::BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL)
{
  char *url = simple_unicode_to_char(aURL);
  gtk_signal_emit_by_name(GTK_OBJECT(mozilla), "begin_load_url", url);
  if (url)
    free(url);
  return NS_OK;
}

NS_IMETHODIMP
GtkMozillaContainer::EndLoadURL(nsIWebShell* aShell,
				const PRUnichar* aURL, PRInt32 aStatus)
{
  char *url = simple_unicode_to_char(aURL);
  gtk_signal_emit_by_name(GTK_OBJECT(mozilla), "end_load_url",
			  url, (gint) aStatus);
  if (url)
    free(url);
  return NS_OK;
}

NS_IMETHODIMP
GtkMozillaContainer::ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aProgress,
                                                                   PRInt32 aProgressMax)
{
  printf("Progress: %d (0..%d)\n", aProgress, aProgressMax);
  return NS_OK;
}

NS_IMETHODIMP
GtkMozillaContainer::NewWebShell(PRUint32 aChromeMask, PRBool aVisible, nsIWebShell *&aNewWebShell)
{
  printf("NewWebShell\n");
  aNewWebShell = nsnull;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GtkMozillaContainer::FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult)
{
  printf("FindWebShellWithName\n");
  aResult = nsnull;
  if (NS_OK == mWebShell->FindChildWithName(aName, aResult)) {
    if (nsnull != aResult) {
      return NS_OK;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
GtkMozillaContainer::ContentShellAdded(nsIWebShell* aChildShell, nsIContent* frameNode)
{
  printf("ContentShellAdded\n");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GtkMozillaContainer::CreatePopup(nsIDOMElement* aElement, nsIDOMElement* aPopupContent, 
	    PRInt32 aXPos, PRInt32 aYPos, 
	    const nsString& aPopupType, const nsString& aPopupAlignment,
	    nsIDOMWindow* aWindow)
{
  printf("CreatePopup\n");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GtkMozillaContainer::CanCreateNewWebShell(PRBool& aResult)
{
  printf("CanCreateNewWebShell\n");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GtkMozillaContainer::SetNewWebShellInfo(const nsString& aName, const nsString& anURL, 
                              nsIWebShell* aOpenerShell, PRUint32 aChromeMask,
                              nsIWebShell** aNewShell, nsIWebShell** anInnerShell)
{
  printf("SetNewWebShellInfo\n");
  return NS_ERROR_FAILURE;
}
  
NS_IMETHODIMP
GtkMozillaContainer::ChildShellAdded(nsIWebShell** aChildShell, nsIContent* frameNode)
{
  printf("ChildShellAdded\n");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GtkMozillaContainer::FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken)
{
  printf("FocusAvailable\n");
  return NS_ERROR_FAILURE;
}
