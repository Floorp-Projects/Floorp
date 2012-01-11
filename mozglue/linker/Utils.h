/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Utils_h
#define Utils_h

#include <stdint.h>

/**
 * On architectures that are little endian and that support unaligned reads,
 * we can use direct type, but on others, we want to have a special class
 * to handle conversion and alignment issues.
 */
#if defined(__i386__) || defined(__x86_64__)
typedef uint16_t le_uint16;
typedef uint32_t le_uint32;
#else

/**
 * Template that allows to find an unsigned int type from a (computed) bit size
 */
template <int s> struct UInt { };
template <> struct UInt<16> { typedef uint16_t Type; };
template <> struct UInt<32> { typedef uint32_t Type; };

/**
 * Template to read 2 n-bit sized words as a 2*n-bit sized word, doing
 * conversion from little endian and avoiding alignment issues.
 */
template <typename T>
class le_to_cpu
{
public:
  operator typename UInt<16 * sizeof(T)>::Type() const
  {
    return (b << (sizeof(T) * 8)) | a;
  }
private:
  T a, b;
};

/**
 * Type definitions
 */
typedef le_to_cpu<unsigned char> le_uint16;
typedef le_to_cpu<le_uint16> le_uint32;
#endif

/**
 * AutoCloseFD is a RAII wrapper for POSIX file descriptors
 */
class AutoCloseFD
{
public:
  AutoCloseFD(): fd(-1) { }
  AutoCloseFD(int fd): fd(fd) { }
  ~AutoCloseFD()
  {
    if (fd != -1)
      close(fd);
  }

  operator int() const
  {
    return fd;
  }

  int forget()
  {
    int _fd = fd;
    fd = -1;
    return _fd;
  }

  bool operator ==(int other) const
  {
    return fd == other;
  }

  int operator =(int other)
  {
    if (fd != -1)
      close(fd);
    fd = other;
    return fd;
  }

private:
  int fd;
};

#endif /* Utils_h */
