/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Interposing getline because of
 * https://bugzilla.mozilla.org/show_bug.cgi?id=914190
 */

#ifdef __ANDROID__

#  include <cstdlib>
#  include <cstdio>
#  include <cerrno>

#  include <limits>

namespace {

// RAII on file locking.
class FileLocker {
  FILE* stream;

 public:
  explicit FileLocker(FILE* stream) : stream(stream) { flockfile(stream); }
  ~FileLocker() { funlockfile(stream); }
};

ssize_t internal_getdelim(char** __restrict lineptr, size_t* __restrict n,
                          int delim, FILE* __restrict stream) {
  constexpr size_t n_default = 64;
  constexpr size_t n_max =
      std::numeric_limits<ssize_t>::max() < std::numeric_limits<size_t>::max()
          ? (size_t)std::numeric_limits<ssize_t>::max() + 1
          : std::numeric_limits<size_t>::max();
  constexpr size_t n_limit = 2 * ((n_max - 1) / 3);

  if (!lineptr || !n || !stream) {
    errno = EINVAL;
    return -1;
  }

  // Lock the file so that we can us unlocked getc in the inner loop.
  FileLocker fl(stream);

  if (!*lineptr || *n == 0) {
    *n = n_default;
    if (auto* new_lineptr = reinterpret_cast<char*>(realloc(*lineptr, *n))) {
      *lineptr = new_lineptr;
    } else {
      errno = ENOMEM;
      return -1;
    }
  }

  ssize_t result;
  size_t cur_len = 0;

  while (true) {
    // Retrieve an extra char.
    int i = getc_unlocked(stream);
    if (i == EOF) {
      result = -1;
      break;
    }

    // Eventually grow the buffer.
    if (cur_len + 1 >= *n) {
      size_t needed = *n >= n_limit ? n_max : 3 * *n / 2 + 1;

      if (cur_len + 1 >= needed) {
        errno = EOVERFLOW;
        return -1;
      }

      if (auto* new_lineptr = (char*)realloc(*lineptr, needed)) {
        *lineptr = new_lineptr;
      } else {
        errno = ENOMEM;
        return -1;
      }
      *n = needed;
    }

    (*lineptr)[cur_len] = i;
    cur_len++;

    if (i == delim) break;
  }
  (*lineptr)[cur_len] = '\0';
  return cur_len ? cur_len : result;
}

}  // namespace

extern "C" {

MFBT_API ssize_t getline(char** __restrict lineptr, size_t* __restrict n,
                         FILE* __restrict stream) {
  return internal_getdelim(lineptr, n, '\n', stream);
}

MFBT_API ssize_t getdelim(char** __restrict lineptr, size_t* __restrict n,
                          int delim, FILE* __restrict stream) {
  return internal_getdelim(lineptr, n, delim, stream);
}

}  // extern "C"

#endif
