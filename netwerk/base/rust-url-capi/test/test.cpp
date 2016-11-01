#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../src/rust-url-capi.h"

class StringContainer
{
public:
  StringContainer()
  {
    mBuffer = nullptr;
    mLength = 0;
  }

  ~StringContainer()
  {
    free(mBuffer);
    mBuffer = nullptr;
  }

  void SetSize(size_t size)
  {
    mLength = size;
    if (mBuffer) {
      mBuffer = (char *)realloc(mBuffer, size);
      return;
    }
    mBuffer = (char *)malloc(size);
  }

  char * GetBuffer()
  {
    return mBuffer;
  }

  void CheckEquals(const char * ref) {
    int32_t refLen = strlen(ref);
    printf("CheckEquals: %s (len:%d)\n", ref, refLen);
    if (refLen != mLength || strncmp(mBuffer, ref, mLength)) {
      printf("\t--- ERROR ---\n");
      printf("Got        : ");
      fwrite(mBuffer, mLength, 1, stdout);
      printf(" (len:%d)\n", mLength);
      exit(-1);
    }
    printf("-> OK\n");
  }
private:
  int32_t mLength;
  char * mBuffer;
};

extern "C" int32_t c_fn_set_size(void * container, size_t size)
{
  ((StringContainer *) container)->SetSize(size);
  return 0;
}

extern "C" char * c_fn_get_buffer(void * container)
{
  return ((StringContainer *) container)->GetBuffer();
}

#define TEST_CALL(func, expected)                  \
{                                                  \
  int32_t code = func;                             \
  printf("%s -> code %d\n", #func, code);          \
  assert(code == expected);                        \
  printf("-> OK\n");                               \
}                                                  \


int main() {
  // Create URL
  rusturl_ptr url = rusturl_new("http://example.com/path/some/file.txt",
                                strlen("http://example.com/path/some/file.txt"));
  assert(url); // Check we have a URL

  StringContainer container;

  TEST_CALL(rusturl_get_spec(url, &container), 0);
  container.CheckEquals("http://example.com/path/some/file.txt");
  TEST_CALL(rusturl_set_host(url, "test.com", strlen("test.com")), 0);
  TEST_CALL(rusturl_get_host(url, &container), 0);
  container.CheckEquals("test.com");
  TEST_CALL(rusturl_get_path(url, &container), 0);
  container.CheckEquals("/path/some/file.txt");
  TEST_CALL(rusturl_set_path(url, "hello/../else.txt", strlen("hello/../else.txt")), 0);
  TEST_CALL(rusturl_get_path(url, &container), 0);
  container.CheckEquals("/else.txt");
  TEST_CALL(rusturl_resolve(url, "./bla/file.txt", strlen("./bla/file.txt"), &container), 0);
  container.CheckEquals("http://test.com/bla/file.txt");
  TEST_CALL(rusturl_get_scheme(url, &container), 0);
  container.CheckEquals("http");
  TEST_CALL(rusturl_set_username(url, "user", strlen("user")), 0);
  TEST_CALL(rusturl_get_username(url, &container), 0);
  container.CheckEquals("user");
  TEST_CALL(rusturl_get_spec(url, &container), 0);
  container.CheckEquals("http://user@test.com/else.txt");
  TEST_CALL(rusturl_set_password(url, "pass", strlen("pass")), 0);
  TEST_CALL(rusturl_get_password(url, &container), 0);
  container.CheckEquals("pass");
  TEST_CALL(rusturl_get_spec(url, &container), 0);
  container.CheckEquals("http://user:pass@test.com/else.txt");
  TEST_CALL(rusturl_set_username(url, "", strlen("")), 0);
  TEST_CALL(rusturl_set_password(url, "", strlen("")), 0);
  TEST_CALL(rusturl_get_spec(url, &container), 0);
  container.CheckEquals("http://test.com/else.txt");
  TEST_CALL(rusturl_set_host_and_port(url, "example.org:1234", strlen("example.org:1234")), 0);
  TEST_CALL(rusturl_get_host(url, &container), 0);
  container.CheckEquals("example.org");
  assert(rusturl_get_port(url) == 1234);
  TEST_CALL(rusturl_set_port(url, "9090", strlen("9090")), 0);
  assert(rusturl_get_port(url) == 9090);
  TEST_CALL(rusturl_set_query(url, "x=1", strlen("x=1")), 0);
  TEST_CALL(rusturl_get_query(url, &container), 0);
  container.CheckEquals("x=1");
  TEST_CALL(rusturl_set_fragment(url, "fragment", strlen("fragment")), 0);
  TEST_CALL(rusturl_get_fragment(url, &container), 0);
  container.CheckEquals("fragment");
  TEST_CALL(rusturl_get_spec(url, &container), 0);
  container.CheckEquals("http://example.org:9090/else.txt?x=1#fragment");

  // Free the URL
  rusturl_free(url);

  url = rusturl_new("http://example.com/#",
                                strlen("http://example.com/#"));
  assert(url); // Check we have a URL

  assert(rusturl_has_fragment(url) == 1);
  TEST_CALL(rusturl_set_fragment(url, "", 0), 0);
  assert(rusturl_has_fragment(url) == 0);
  TEST_CALL(rusturl_get_spec(url, &container), 0);
  container.CheckEquals("http://example.com/");

  rusturl_free(url);

  printf("SUCCESS\n");
  return 0;
}