/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Test ReadSysFile() */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "FileUtils.h"

#include "gtest/gtest.h"

namespace mozilla {

#ifdef ReadSysFile_PRESENT

/**
 * Create a file with the specified contents.
 */
static bool
WriteFile(
  const char* aFilename,
  const void* aContents,
  size_t aContentsLen)
{
  int fd;
  ssize_t ret;
  size_t offt;

  fd = TEMP_FAILURE_RETRY(
    open(aFilename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR));
  if (fd == -1) {
    fprintf(stderr, "open(): %s: %s\n", aFilename, strerror(errno));
    return false;
  }

  offt = 0;
  do {
    ret = TEMP_FAILURE_RETRY(write(fd, (char*)aContents + offt, aContentsLen - offt));
    if (ret == -1) {
      fprintf(stderr, "write(): %s: %s\n", aFilename, strerror(errno));
      close(fd);
      return false;
    }
    offt += ret;
  } while (offt < aContentsLen);

  ret = TEMP_FAILURE_RETRY(close(fd));
  if (ret == -1) {
    fprintf(stderr, "close(): %s: %s\n", aFilename, strerror(errno));
    return false;
  }
  return true;
}

TEST(ReadSysFile, Nonexistent) {
  bool ret;
  int errno_saved;

  ret = ReadSysFile("/nonexistent", NULL, 0);
  errno_saved = errno;

  ASSERT_FALSE(ret);
  ASSERT_EQ(errno_saved, ENOENT);
}

TEST(ReadSysFile, Main) {
  /* Use a different file name for each test since different tests could be
  executed concurrently. */
  static const char* fn = "TestReadSysFileMain";
  /* If we have a file which contains "abcd" and we read it with ReadSysFile(),
  providing a buffer of size 10 bytes, we would expect 5 bytes to be written
  to that buffer: "abcd\0". */
  struct {
    /* input (file contents), e.g. "abcd" */
    const char* input;
    /* pretended output buffer size, e.g. 10; the actual buffer is larger
    and we check if anything was written past the end of the allowed length */
    size_t output_size;
    /* expected number of bytes written to the output buffer, including the
    terminating '\0', e.g. 5 */
    size_t output_len;
    /* expected output buffer contents, e.g. "abcd\0", the first output_len
    bytes of the output buffer should match the first 'output_len' bytes from
    'output', the rest of the output buffer should be untouched. */
    const char* output;
  } tests[] = {
    /* No new lines */
    {"", 0, 0, ""},
    {"", 1, 1, "\0"}, /* \0 is redundant, but we write it for clarity */
    {"", 9, 1, "\0"},

    {"a", 0, 0, ""},
    {"a", 1, 1, "\0"},
    {"a", 2, 2, "a\0"},
    {"a", 9, 2, "a\0"},

    {"abcd", 0, 0, ""},
    {"abcd", 1, 1, "\0"},
    {"abcd", 2, 2, "a\0"},
    {"abcd", 3, 3, "ab\0"},
    {"abcd", 4, 4, "abc\0"},
    {"abcd", 5, 5, "abcd\0"},
    {"abcd", 9, 5, "abcd\0"},

    /* A single trailing new line */
    {"\n", 0, 0, ""},
    {"\n", 1, 1, "\0"},
    {"\n", 2, 1, "\0"},
    {"\n", 9, 1, "\0"},

    {"a\n", 0, 0, ""},
    {"a\n", 1, 1, "\0"},
    {"a\n", 2, 2, "a\0"},
    {"a\n", 3, 2, "a\0"},
    {"a\n", 9, 2, "a\0"},

    {"abcd\n", 0, 0, ""},
    {"abcd\n", 1, 1, "\0"},
    {"abcd\n", 2, 2, "a\0"},
    {"abcd\n", 3, 3, "ab\0"},
    {"abcd\n", 4, 4, "abc\0"},
    {"abcd\n", 5, 5, "abcd\0"},
    {"abcd\n", 6, 5, "abcd\0"},
    {"abcd\n", 9, 5, "abcd\0"},

    /* Multiple trailing new lines */
    {"\n\n", 0, 0, ""},
    {"\n\n", 1, 1, "\0"},
    {"\n\n", 2, 2, "\n\0"},
    {"\n\n", 3, 2, "\n\0"},
    {"\n\n", 9, 2, "\n\0"},

    {"a\n\n", 0, 0, ""},
    {"a\n\n", 1, 1, "\0"},
    {"a\n\n", 2, 2, "a\0"},
    {"a\n\n", 3, 3, "a\n\0"},
    {"a\n\n", 4, 3, "a\n\0"},
    {"a\n\n", 9, 3, "a\n\0"},

    {"abcd\n\n", 0, 0, ""},
    {"abcd\n\n", 1, 1, "\0"},
    {"abcd\n\n", 2, 2, "a\0"},
    {"abcd\n\n", 3, 3, "ab\0"},
    {"abcd\n\n", 4, 4, "abc\0"},
    {"abcd\n\n", 5, 5, "abcd\0"},
    {"abcd\n\n", 6, 6, "abcd\n\0"},
    {"abcd\n\n", 7, 6, "abcd\n\0"},
    {"abcd\n\n", 9, 6, "abcd\n\0"},

    /* New line in the middle */
    {"ab\ncd", 9, 6, "ab\ncd\0"},
  };

  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    ASSERT_TRUE(WriteFile(fn, tests[i].input, strlen(tests[i].input)));
    /* Leave the file to exist if some of the assertions fail. */

    char buf[128];
    static const char unmodified = 'X';

    memset(buf, unmodified, sizeof(buf));

    ASSERT_TRUE(ReadSysFile(fn, buf, tests[i].output_size));

    if (tests[i].output_size == 0) {
      /* The buffer must be unmodified. We check only the first byte. */
      ASSERT_EQ(unmodified, buf[0]);
    } else {
      ASSERT_EQ(tests[i].output_len, strlen(buf) + 1);
      ASSERT_STREQ(tests[i].output, buf);
      /* Check that the first byte after the trailing '\0' has not been
      modified. */
      ASSERT_EQ(unmodified, buf[tests[i].output_len]);
    }
  }

  unlink(fn);
}

TEST(ReadSysFile, Int) {
  static const char* fn = "TestReadSysFileInt";
  struct {
    /* input (file contents), e.g. "5" */
    const char* input;
    /* expected return value, if false, then the output is not checked */
    bool ret;
    /* expected result */
    int output;
  } tests[] = {
    {"0", true, 0},
    {"00", true, 0},
    {"1", true, 1},
    {"5", true, 5},
    {"55", true, 55},

    {" 123", true, 123},
    {"123 ", true, 123},
    {" 123 ", true, 123},
    {"123\n", true, 123},

    {"", false, 0},
    {" ", false, 0},
    {"a", false, 0},

    {"-1", true, -1},
    {" -456 ", true, -456},
    {" -78.9 ", true, -78},
  };

  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    ASSERT_TRUE(WriteFile(fn, tests[i].input, strlen(tests[i].input)));
    /* Leave the file to exist if some of the assertions fail. */

    bool ret;
    int output = 424242;

    ret = ReadSysFile(fn, &output);

    ASSERT_EQ(tests[i].ret, ret);

    if (ret) {
      ASSERT_EQ(tests[i].output, output);
    }
  }

  unlink(fn);
}

TEST(ReadSysFile, Bool) {
  static const char* fn = "TestReadSysFileBool";
  struct {
    /* input (file contents), e.g. "1" */
    const char* input;
    /* expected return value */
    bool ret;
    /* expected result */
    bool output;
  } tests[] = {
    {"0", true, false},
    {"00", true, false},
    {"1", true, true},
    {"5", true, true},
    {"23", true, true},
    {"-1", true, true},

    {"", false, true /* unused */},
  };

  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    ASSERT_TRUE(WriteFile(fn, tests[i].input, strlen(tests[i].input)));
    /* Leave the file to exist if some of the assertions fail. */

    bool ret;
    bool output;

    ret = ReadSysFile(fn, &output);

    ASSERT_EQ(tests[i].ret, ret);

    if (ret) {
      ASSERT_EQ(tests[i].output, output);
    }
  }

  unlink(fn);
}

#endif /* ReadSysFile_PRESENT */

}
