// Test06.cpp

#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIBaseWindow.h"
#include "nsCOMPtr.h"

NS_DEF_PTR(nsPIDOMWindow);
NS_DEF_PTR(nsIBaseWindow);

  /*
    Windows:
      nsCOMPtr_optimized           176
      nsCOMPtr_as_found            181
      nsCOMPtr_optimized*          182
      nsCOMPtr02*                  184
      nsCOMPtr02                   187
      nsCOMPtr02*                  188
      nsCOMPtr03                   189
      raw_optimized, nsCOMPtr00    191
      nsCOMPtr00*                  199
      nsCOMPtr_as_found*           201
      raw                          214

    Macintosh:
      nsCOMPtr_optimized           300    (1.0000)
      nsCOMPtr02                   320    (1.0667)  i.e., 6.67% bigger than nsCOMPtr_optimized
      nsCOMPtr00                   328    (1.0933)
      raw_optimized, nsCOMPtr03    332    (1.1067)
      nsCOMPtr_as_found            344    (1.1467)
      raw                          388    (1.2933)

  */


void // nsresult
Test06_raw(nsIDOMWindow* aDOMWindow, nsIBaseWindow** aBaseWindow)
    // m388, w214
{
//  if (!aDOMWindow)
//    return NS_ERROR_NULL_POINTER;
  nsPIDOMWindow* window = 0;
  nsresult status = aDOMWindow->QueryInterface(NS_GET_IID(nsPIDOMWindow), (void**)&window);
  nsIDocShell* docShell = 0;
  if (window)
    window->GetDocShell(&docShell);
  nsIWebShell* rootWebShell = 0;
  NS_IF_RELEASE(rootWebShell);
  NS_IF_RELEASE(docShell);
  NS_IF_RELEASE(window);
//    return status;
}

void // nsresult
Test06_raw_optimized(nsIDOMWindow* aDOMWindow, nsIBaseWindow** aBaseWindow)
    // m332, w191
{
//  if (!aDOMWindow)
//    return NS_ERROR_NULL_POINTER;
  (*aBaseWindow) = 0;
  nsPIDOMWindow* window;
  nsresult status = aDOMWindow->QueryInterface(NS_GET_IID(nsPIDOMWindow), (void**)&window);
  if (NS_SUCCEEDED(status)) {
    nsIDocShell* docShell = 0;
    window->GetDocShell(&docShell);
    if (docShell) {
      NS_RELEASE(docShell);
    }
    NS_RELEASE(window);
  }
//  return status;
}

void
Test06_nsCOMPtr_as_found(nsIDOMWindow* aDOMWindow, nsCOMPtr<nsIBaseWindow>* aBaseWindow)
    // m344, w181/201
{
//  if (!aDOMWindow)
//    return;
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aDOMWindow);
  nsCOMPtr<nsIDocShell> docShell;
  if (window)
    window->GetDocShell(getter_AddRefs(docShell));  
}

void // nsresult
Test06_nsCOMPtr00(nsIDOMWindow* aDOMWindow, nsIBaseWindow** aBaseWindow)
    // m328, w191/199
{
//  if (!aDOMWindow)
//    return NS_ERROR_NULL_POINTER;
  nsresult status;
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aDOMWindow, &status);
  nsIDocShell* temp0 = 0;
  if (window)
    window->GetDocShell(&temp0);
  nsCOMPtr<nsIDocShell> docShell = dont_AddRef(temp0);
  (*aBaseWindow) = 0;
//    return status;
}

void // nsresult
Test06_nsCOMPtr_optimized(nsIDOMWindow* aDOMWindow, nsCOMPtr<nsIBaseWindow>* aBaseWindow)
    // m300, w176/182
{
//    if (!aDOMWindow)
//      return NS_ERROR_NULL_POINTER;
  nsresult status;
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aDOMWindow, &status);
  nsIDocShell* temp0 = 0;
  if (window)
    window->GetDocShell(&temp0);
  (*aBaseWindow) = do_QueryInterface(nullptr, &status);
//    return status;
}

void // nsresult
Test06_nsCOMPtr02(nsIDOMWindow* aDOMWindow, nsIBaseWindow** aBaseWindow)
    // m320, w187/184
{
//    if (!aDOMWindow)
//      return NS_ERROR_NULL_POINTER;
  (*aBaseWindow) = 0;
  nsresult status;
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aDOMWindow, &status);
  if (window) {
    nsIDocShell* temp0;
    window->GetDocShell(&temp0);
  }
//    return status;
}

void // nsresult
Test06_nsCOMPtr03(nsIDOMWindow* aDOMWindow, nsCOMPtr<nsIBaseWindow>* aBaseWindow)
    // m332, w189/188
{
//    if (!aDOMWindow)
//      return NS_ERROR_NULL_POINTER;
  (*aBaseWindow) = 0;
  nsresult status;
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aDOMWindow, &status);
  if (window) {
    nsIDocShell* temp0;
    window->GetDocShell(&temp0);
    nsCOMPtr<nsIDocShell> docShell = dont_AddRef(temp0);
    if (docShell) {
    }
  }
//    return status;
}
