#include "TestHarness.h"

#include "nsISettingsService.h"

#include "nsCOMPtr.h"
#include "nsIXPConnect.h"
#include "nsIObserver.h"
#include "nsIThread.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/Services.h"

#define XPCOM_SHUTDOWN        "xpcom-shutdown"
#define MOZSETTINGS_CHANGED   "mozsettings-changed"

#define TEST_OBSERVER_KEY     "test.observer.key"
#define TEST_OBSERVER_VALUE   true
#define TEST_OBSERVER_MESSAGE "test.observer.message"

using namespace mozilla;

static int callbackCount = 10;
static int observerCount = 2;
static int errors = 0;

#define CHECK(x) \
  _doCheck(x, #x, __LINE__)

#define CHECK_MSG(x, msg) \
  _doCheck(x, msg, __LINE__)

static void _doCheck(bool cond, const char *msg, int line) {
  if (cond) return;
  fprintf(stderr, "FAIL: line %d: %s\n", line, msg);
  errors++;
}

static bool VerifyJSValueIsString(
  JSContext *cx, const JS::Value &value, const char *string) {
  JSBool match;
  if (!value.isString() ||
      !JS_StringEqualsAscii(cx, value.toString(), string, &match) ||
      (match != JS_TRUE)) {
    return false;
  }
  return true;
}

typedef nsresult(*TestFuncPtr)();

class SettingsServiceCallback MOZ_FINAL : public nsISettingsServiceCallback
{
public:
  NS_DECL_ISUPPORTS

  SettingsServiceCallback() { }

  NS_IMETHOD Handle(const nsAString &name, const JS::Value &result) {
    if (callbackCount == 9) {
      CHECK(JSVAL_IS_BOOLEAN(result));
      CHECK(!!JSVAL_TO_BOOLEAN(result) == true);
      passed("boolean");
    } else if (callbackCount == 7) {
      CHECK(JSVAL_IS_BOOLEAN(result));
      CHECK(!!JSVAL_TO_BOOLEAN(result) == false);
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
      CHECK(JSVAL_IS_NULL(result));
      passed("null");
    }
    callbackCount--;
    return NS_OK;
  }

  NS_IMETHOD HandleError(const nsAString &name) {
    fprintf(stderr, "HANDLE Error! %s\n", NS_LossyConvertUTF16toASCII(name).get());
    errors++;
    return NS_OK;
  }
};

NS_IMPL_THREADSAFE_ISUPPORTS1(SettingsServiceCallback, nsISettingsServiceCallback)

class TestSettingsObserver MOZ_FINAL : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  TestSettingsObserver() {
    // Setup an observer to watch changes to the setting.
    nsCOMPtr<nsIObserverService> observerService =
      services::GetObserverService();
    if (!observerService) {
      CHECK_MSG(false, "GetObserverService failed");
      return;
    }

    nsresult rv;
    rv = observerService->AddObserver(this, XPCOM_SHUTDOWN, false);
    if (NS_FAILED(rv)) {
      CHECK_MSG(false,"AddObserver failed");
      return;
    }
    rv = observerService->AddObserver(this, MOZSETTINGS_CHANGED, false);
    if (NS_FAILED(rv)) {
      CHECK_MSG(false,"AddObserver failed");
      return;
    }
  }

  ~TestSettingsObserver() {}
};

NS_IMPL_ISUPPORTS1(TestSettingsObserver, nsIObserver)

NS_IMETHODIMP
TestSettingsObserver::Observe(nsISupports *aSubject,
                              const char *aTopic,
                              const PRUnichar *aData ) {
  // Check if receiving the "xpcom-shutdown" event.
  if (strcmp(aTopic, XPCOM_SHUTDOWN) == 0) {
    nsCOMPtr<nsIObserverService> observerService =
      services::GetObserverService();
    if (!observerService) {
      CHECK_MSG(false, "GetObserverService failed");
      return NS_OK;
    }
    observerService->RemoveObserver(this, XPCOM_SHUTDOWN);
    observerService->RemoveObserver(this, MOZSETTINGS_CHANGED);
    return NS_OK;
  }

  // Check if receiving the "mozsettings-changed" event.
  if (strcmp(aTopic, MOZSETTINGS_CHANGED) != 0) {
    CHECK_MSG(false, "Got non-mozsettings-changed event");
    return NS_OK;
  }

  // Get the safe JS context.
  nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
  JSContext *cx = xpc->GetSafeJSContext();
  if (!cx) {
    CHECK_MSG(false, "Failed to GetSafeJSContext");
    return NS_OK;
  }

  // Parse the JSON data.
  nsDependentString dataStr(aData);
  JS::Rooted<JS::Value> data(cx);
  if (!JS_ParseJSON(cx, dataStr.get(), dataStr.Length(), &data) ||
      !data.isObject()) {
    CHECK_MSG(false, "Failed to get the data");
    return NS_OK;
  }
  JSObject &obj(data.toObject());

  // Get and check the 'key' value. Should be TEST_OBSERVER_KEY;
  // otherwise, skip it (they are from other cases we don't care).
  JS::Value key;
  if (!JS_GetProperty(cx, &obj, "key", &key)) {
    CHECK_MSG(false, "Failed to get the key");
    return NS_OK;
  }
  if (!VerifyJSValueIsString(cx, key, TEST_OBSERVER_KEY)) {
    return NS_OK;
  }

  // Get and check the 'value' value. Should be TEST_OBSERVER_VALUE.
  JS::Value value;
  if (!JS_GetProperty(cx, &obj, "value", &value)) {
    CHECK_MSG(false, "Failed to get the value");
    return NS_OK;
  }
  if (!value.isBoolean() || (value.toBoolean() != TEST_OBSERVER_VALUE)) {
    CHECK_MSG(false, "The 'value' value is wrong");
    return NS_OK;
  }

  // Get and check the 'message' value.
  // Should be null for case #1 and TEST_OBSERVER_MESSAGE for case #2.
  JS::Value message;
  if (!JS_GetProperty(cx, &obj, "message", &message)) {
    CHECK_MSG(false, "Failed to get the message");
    return NS_OK;
  }
  if ((observerCount == 2 && !message.isNull()) ||
      (observerCount == 1 && !VerifyJSValueIsString(cx, message, TEST_OBSERVER_MESSAGE))) {
    CHECK_MSG(false, "The 'message' value is wrong");
    return NS_OK;
  }

  --observerCount;
  passed("observer");
  return NS_OK;
}

nsresult
TestSettingsAPI()
{
  nsresult rv;
  nsCOMPtr<nsISettingsService> settingsService =
    do_CreateInstance("@mozilla.org/settingsService;1", &rv);
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
  iss->CreateLock(getter_AddRefs(lock));

  nsCOMPtr<nsISettingsServiceLock> lock1;
  iss->CreateLock(getter_AddRefs(lock1));

  lock->Set("asdf", BOOLEAN_TO_JSVAL(true), cb0, nullptr);
  lock1->Get("asdf", cb1);
  lock->Get("asdf", cb2);
  lock->Set("asdf", BOOLEAN_TO_JSVAL(false), cb3, nullptr);
  lock->Get("asdf", cb4);
  lock->Set("int", INT_TO_JSVAL(9), cb5, nullptr);
  lock->Get("int", cb6);
  lock->Set("doub", DOUBLE_TO_JSVAL(9.4), cb7, nullptr);
  lock->Get("doub", cb8);
  lock1->Get("asdfxxx", cb9);

  // The followings test if the observer can receive correct settings.
  // Case #1 won't carry any message; case #2 will carry TEST_OBSERVER_MESSAGE.
  nsCOMPtr<nsISettingsServiceLock> lock2;
    iss->CreateLock(getter_AddRefs(lock2));
  lock2->Set(TEST_OBSERVER_KEY,
             BOOLEAN_TO_JSVAL(TEST_OBSERVER_VALUE),
             nullptr, nullptr);
  lock2->Set(TEST_OBSERVER_KEY,
             BOOLEAN_TO_JSVAL(TEST_OBSERVER_VALUE),
             nullptr, TEST_OBSERVER_MESSAGE);

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

  nsRefPtr<TestSettingsObserver> sTestSettingsObserver =
    new TestSettingsObserver();

  static TestFuncPtr testsToRun[] = {
    TestSettingsAPI
  };
  static uint32_t testCount = sizeof(testsToRun) / sizeof(testsToRun[0]);

  for (uint32_t i = 0; i < testCount; i++) {
    nsresult rv = testsToRun[i]();
    NS_ENSURE_SUCCESS(rv, 1);
  }
  nsCOMPtr<nsIThread> current(do_GetCurrentThread());
  while ((callbackCount > 0 || observerCount > 0) && !errors)
    NS_ProcessNextEvent(current);

  passed("END!");
  return errors;
}
