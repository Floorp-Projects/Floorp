#include "TestHarness.h"

#include "nsISettingsService.h"

#include "nsCOMPtr.h"
#include "nsIThread.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsThreadUtils.h"

using namespace mozilla;

static const char* gFunction = "main";

static int callbackCount = 10;
static int errors = 0;

#define CHECK(x) \
  _doCheck(x, #x, __LINE__)

void _doCheck(bool cond, const char* msg, int line) {
  if (cond) return;
  fprintf(stderr, "FAIL: line %d: %s\n", line, msg);
  errors++;
}

typedef nsresult(*TestFuncPtr)();

class SettingsServiceCallback : public nsISettingsServiceCallback
{
public:
  NS_DECL_ISUPPORTS

  SettingsServiceCallback() { }

  NS_IMETHOD Handle(const nsAString & name, const JS::Value & result, JSContext* cx) {
    if (callbackCount == 9) {
      CHECK(JSVAL_IS_BOOLEAN(result));
      CHECK(JSVAL_TO_BOOLEAN(result) == true);
      passed("boolean");
    } else if (callbackCount == 7) {
      CHECK(JSVAL_IS_BOOLEAN(result));
      CHECK(JSVAL_TO_BOOLEAN(result) == false);
      passed("Lock order");
    } else if (callbackCount == 5) {
      CHECK(JSVAL_IS_INT(result));
      CHECK(JSVAL_TO_INT(result) == 9);
      passed("integer");
    }  else if (callbackCount == 3) {
      CHECK(JSVAL_IS_DOUBLE(result));
      CHECK(JSVAL_TO_DOUBLE(result) == 9.4);
      passed("double");
    } else if (callbackCount == 2) {
      CHECK(JSVAL_IS_BOOLEAN(result));
      CHECK(JSVAL_TO_BOOLEAN(result) == false);
      passed("Lock order");
    } else if (callbackCount == 1) {
      passed("null");
      CHECK(JSVAL_IS_NULL(result));
    }
    callbackCount--;
    return NS_OK;
  };

  NS_IMETHOD HandleError(const nsAString & name, JSContext* cx) {
    fprintf(stderr, "HANDLE Error! %s\n", NS_LossyConvertUTF16toASCII(name).get());
    errors++;
    return NS_OK;
  };
};

NS_IMPL_THREADSAFE_ISUPPORTS1(SettingsServiceCallback, nsISettingsServiceCallback)

nsresult
TestSettingsAPI()
{
  nsresult rv;
  nsCOMPtr<nsISettingsService> settingsService = do_CreateInstance("@mozilla.org/settingsService;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISettingsServiceCallback> cb0 = new SettingsServiceCallback();
  nsCOMPtr<nsISettingsServiceCallback> cb1 = new SettingsServiceCallback();
  nsCOMPtr<nsISettingsServiceCallback> cb2 = new SettingsServiceCallback();
  nsCOMPtr<nsISettingsServiceCallback> cb3 = new SettingsServiceCallback();
  nsCOMPtr<nsISettingsServiceCallback> cb4 = new SettingsServiceCallback();
  nsCOMPtr<nsISettingsServiceCallback> cb5 = new SettingsServiceCallback();
  nsCOMPtr<nsISettingsServiceCallback> cb6 = new SettingsServiceCallback();
  nsCOMPtr<nsISettingsServiceCallback> cb7 = new SettingsServiceCallback();
  nsCOMPtr<nsISettingsServiceCallback> cb8 = new SettingsServiceCallback();
  nsCOMPtr<nsISettingsServiceCallback> cb9 = new SettingsServiceCallback();

  nsCOMPtr<nsISettingsService> iss = do_GetService("@mozilla.org/settingsService;1");
  nsCOMPtr<nsISettingsServiceLock> lock;
  iss->GetLock(getter_AddRefs(lock));

  nsCOMPtr<nsISettingsServiceLock> lock1;
  iss->GetLock(getter_AddRefs(lock1));

  lock->Set("asdf", BOOLEAN_TO_JSVAL(true), cb0);
  lock1->Get("asdf", cb1);
  lock->Get("asdf", cb2);
  lock->Set("asdf", BOOLEAN_TO_JSVAL(false), cb3);
  lock->Get("asdf", cb4);
  lock->Set("int", INT_TO_JSVAL(9), cb5);
  lock->Get("int", cb6);
  lock->Set("doub", DOUBLE_TO_JSVAL(9.4), cb7);
  lock->Get("doub", cb8);
  lock1->Get("asdfxxx", cb9);

  return NS_OK;
}

int main(int argc, char** argv)
{
  passed("Start TestSettingsAPI");
  ScopedXPCOM xpcom("TestSettingsAPI");
  nsCOMPtr<nsIProperties> os =
    do_GetService("@mozilla.org/file/directory_service;1");
  if (!os) {
    fprintf(stderr, "DIRSERVICE NULL!!!!!!\n");
    return 1;
  }

  NS_ENSURE_FALSE(xpcom.failed(), 1);

  static TestFuncPtr testsToRun[] = {
    TestSettingsAPI
  };
  static PRUint32 testCount = sizeof(testsToRun) / sizeof(testsToRun[0]);

  for (PRUint32 i = 0; i < testCount; i++) {
    nsresult rv = testsToRun[i]();
    NS_ENSURE_SUCCESS(rv, 1);
  }
  nsCOMPtr<nsIThread> current(do_GetCurrentThread());
  while (callbackCount > 0)
    NS_ProcessNextEvent(current);

  passed("END!");
  return errors;
}
