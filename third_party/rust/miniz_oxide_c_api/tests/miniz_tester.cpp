// miniz_tester.cpp
// Note: This module is not intended to make a good example, or be used for anything other than testing.
// It's something quick I put together last year to help regression test miniz/tinfl under Linux/Win32/Mac. It's derived from LZHAM's test module.
#ifdef _MSC_VER
#pragma warning (disable:4127) //  warning C4127: conditional expression is constant
#endif

#if defined(__GNUC__)
  // Ensure we get the 64-bit variants of the CRT's file I/O calls
  #ifndef _FILE_OFFSET_BITS
    #define _FILE_OFFSET_BITS 64
  #endif
  #ifndef _LARGEFILE64_SOURCE
    #define _LARGEFILE64_SOURCE 1
  #endif
#endif

#include "miniz.h"
#include "miniz_zip.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <memory.h>
#include <stdarg.h>
#include <malloc.h>
#include <vector>
#include <string>
#include <limits.h>
#include <sys/stat.h>

#include "timer.h"

#define my_max(a,b) (((a) > (b)) ? (a) : (b))
#define my_min(a,b) (((a) < (b)) ? (a) : (b))

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint;

#define TDEFL_PRINT_OUTPUT_PROGRESS

#if defined(WIN32)
   #define WIN32_LEAN_AND_MEAN
   #include <windows.h>
   #define FILE_STAT_STRUCT _stat
   #define FILE_STAT _stat
#else
   #include <unistd.h>
   #define Sleep(ms) usleep(ms*1000)
   #define _aligned_malloc(size, alignment) memalign(alignment, size)
   #define _aligned_free free
   #define fopen fopen64
   #define _fseeki64 fseeko64
   #define _ftelli64 ftello64
   #define _stricmp strcasecmp
   #define FILE_STAT_STRUCT stat64
   #define FILE_STAT stat64
#endif

#ifdef WIN32
#define QUAD_INT_FMT "%I64u"
#else
#define QUAD_INT_FMT "%llu"
#endif

#ifdef _DEBUG
const bool g_is_debug = true;
#else
const bool g_is_debug = false;
#endif

typedef unsigned char uint8;
typedef unsigned int uint;
typedef unsigned int uint32;
typedef unsigned long long uint64;
typedef long long int64;

#define TDEFLTEST_COMP_INPUT_BUFFER_SIZE 1024*1024*2
#define TDEFLTEST_COMP_OUTPUT_BUFFER_SIZE 1024*1024*2
#define TDEFLTEST_DECOMP_INPUT_BUFFER_SIZE 1024*1024*2

static float s_max_small_comp_ratio, s_max_large_comp_ratio;

struct comp_options
{
  comp_options() :
    m_level(7),
    m_unbuffered_decompression(false),
    m_verify_compressed_data(false),
    m_randomize_params(false),
    m_randomize_buffer_sizes(false),
    m_z_strat(Z_DEFAULT_STRATEGY),
    m_random_z_flushing(false),
    m_write_zlib_header(true),
    m_archive_test(false),
    m_write_archives(false)
  {
  }

  void print()
  {
    printf("Level: %u\n", m_level);
    printf("Write zlib header: %u\n", (uint)m_write_zlib_header);
    printf("Unbuffered decompression: %u\n", (uint)m_unbuffered_decompression);
    printf("Verify compressed data: %u\n", (uint)m_verify_compressed_data);
    printf("Randomize parameters: %u\n", m_randomize_params);
    printf("Randomize buffer sizes: %u\n", m_randomize_buffer_sizes);
    printf("Deflate strategy: %u\n", m_z_strat);
    printf("Random Z stream flushing: %u\n", m_random_z_flushing);
    printf("Archive test: %u\n", m_archive_test);
    printf("Write archives: %u\n", m_write_archives);
  }

  uint m_level;
  bool m_unbuffered_decompression;
  bool m_verify_compressed_data;
  bool m_randomize_params;
  bool m_randomize_buffer_sizes;
  uint m_z_strat;
  bool m_random_z_flushing;
  bool m_write_zlib_header;
  bool m_archive_test;
  bool m_write_archives;
};

#define RND_SHR3(x)  (x ^= (x << 17), x ^= (x >> 13), x ^= (x << 5))

#if 0
static void random_fill(uint8 *pDst, size_t len, uint32 x)
{
  x ^= (x << 16);
  if (!x) x++;

  while (len)
  {
    RND_SHR3(x); uint64 l0 = x & 0xFFF;
    RND_SHR3(x); uint64 l1 = x & 0xFFF;
    RND_SHR3(x); uint64 l2 = x & 0xFFF;
    RND_SHR3(x); uint c = x;

    uint l = (uint)(((l0*l1*l2)/(16769025ULL) * 32) / 4095);
    l = (uint)my_max(1,my_min(l, len));
    len -= l;

    while (l--)
    {
      *pDst++ = (uint8)c;
    }

    if (((int)x < 0) && len)
    {
      *pDst++ = 0;
      len--;
    }
  }
}
#endif

static void print_usage()
{
  printf("Usage: [options] [mode] inpath/infile [outfile]\n");
  printf("\n");
  printf("Modes:\n");
  printf("c - Compress \"infile\" to \"outfile\"\n");
  printf("d - Decompress \"infile\" to \"outfile\"\n");
  printf("a - Recursively compress all files under \"inpath\"\n");
  printf("r - Archive decompression test\n");
  printf("\n");
  printf("Options:\n");
  printf("-m[0-10] - Compression level: 0=fastest (Huffman only), 9=best (10=uber)\n");
  printf("-u - Use unbuffered decompression on files that can fit into memory.\n");
  printf("     Unbuffered decompression is faster, but may have more I/O overhead.\n");
  printf("-v - Immediately decompress compressed file after compression for verification.\n");
  printf("-z - Do not write zlib header\n");
  printf("-r - Randomize parameters during recursive testing\n");
  printf("-b - Randomize input/output buffer sizes\n");
  printf("-h - Use random z_flushing\n");
  printf("-x# - Set rand() seed to value\n");
  printf("-t# - Set z_strategy to value [0-4]\n");
  printf("-a - Create single-file archives instead of files during testing\n");
  printf("-w - Test archive cloning\n");
}

static void print_error(const char *pMsg, ...)
{
  char buf[1024];

  va_list args;
  va_start(args, pMsg);
  vsnprintf(buf, sizeof(buf), pMsg, args);
  va_end(args);

  buf[sizeof(buf) - 1] = '\0';

  fprintf(stderr, "Error: %s", buf);
}

static FILE* open_file_with_retries(const char *pFilename, const char* pMode)
{
  const uint cNumRetries = 8;
  for (uint i = 0; i < cNumRetries; i++)
  {
    FILE* pFile = fopen(pFilename, pMode);
    if (pFile)
      return pFile;
    Sleep(250);
  }
  return NULL;
}

static bool ensure_file_exists_and_is_readable(const char *pFilename)
{
  FILE *p = fopen(pFilename, "rb");
  if (!p)
    return false;

  _fseeki64(p, 0, SEEK_END);
  uint64 src_file_size = _ftelli64(p);
  _fseeki64(p, 0, SEEK_SET);

  if (src_file_size)
  {
    char buf[1];
    if (fread(buf, 1, 1, p) != 1)
    {
      fclose(p);
      return false;
    }
  }
  fclose(p);
  return true;
}

static bool ensure_file_is_writable(const char *pFilename)
{
  const int cNumRetries = 8;
  for (int i = 0; i < cNumRetries; i++)
  {
    FILE *pFile = fopen(pFilename, "wb");
    if (pFile)
    {
      fclose(pFile);
      return true;
    }
    Sleep(250);
  }
  return false;
}

static int simple_test1(const comp_options &options)
{
  (void)options;

  size_t cmp_len = 0;

  const char *p = "This is a test.This is a test.This is a test.1234567This is a test.This is a test.123456";
  size_t uncomp_len = strlen(p);

  void *pComp_data = tdefl_compress_mem_to_heap(p, uncomp_len, &cmp_len, TDEFL_WRITE_ZLIB_HEADER);
  if (!pComp_data)
  {
    free(pComp_data);
    print_error("Compression test failed!\n");
    return EXIT_FAILURE;
  }

  printf("Uncompressed size: %u\nCompressed size: %u\n", (uint)uncomp_len, (uint)cmp_len);

  size_t decomp_len = 0;
  void *pDecomp_data = tinfl_decompress_mem_to_heap(pComp_data, cmp_len, &decomp_len, TINFL_FLAG_PARSE_ZLIB_HEADER);

  if ((!pDecomp_data) || (decomp_len != uncomp_len) || (memcmp(pDecomp_data, p, uncomp_len)))
  {
    free(pComp_data);
    free(pDecomp_data);
    print_error("Compression test failed!\n");
    return EXIT_FAILURE;
  }

  printf("Low-level API compression test succeeded.\n");

  free(pComp_data);
  free(pDecomp_data);

  return EXIT_SUCCESS;
}

static int simple_test2(const comp_options &options)
{
  (void)options;

  uint8 cmp_buf[1024], decomp_buf[1024];
  uLong cmp_len = sizeof(cmp_buf);

  const char *p = "This is a test.This is a test.This is a test.1234567This is a test.This is a test.123456";
  uLong uncomp_len = (uLong)strlen(p);

  int status = compress(cmp_buf, &cmp_len, (const uint8*)p, uncomp_len);
  if (status != Z_OK)
  {
    print_error("Compression test failed!\n");
    return EXIT_FAILURE;
  }

  printf("Uncompressed size: %u\nCompressed size: %u\n", (uint)uncomp_len, (uint)cmp_len);

  if (cmp_len > compressBound(uncomp_len))
  {
    print_error("compressBound() returned bogus result\n");
    return EXIT_FAILURE;
  }

  uLong decomp_len = sizeof(decomp_buf);
  status = uncompress(decomp_buf, &decomp_len, cmp_buf, cmp_len);;

  if ((status != Z_OK) || (decomp_len != uncomp_len) || (memcmp(decomp_buf, p, uncomp_len)))
  {
    print_error("Compression test failed!\n");
    return EXIT_FAILURE;
  }

  printf("zlib API compression test succeeded.\n");

  return EXIT_SUCCESS;
}

static bool compress_file_zlib(const char* pSrc_filename, const char *pDst_filename, const comp_options &options)
{
  printf("Testing: Streaming zlib compression\n");

  FILE *pInFile = fopen(pSrc_filename, "rb");
  if (!pInFile)
  {
    print_error("Unable to read file: %s\n", pSrc_filename);
    return false;
  }

  FILE *pOutFile = fopen(pDst_filename, "wb");
  if (!pOutFile)
  {
    print_error("Unable to create file: %s\n", pDst_filename);
    return false;
  }

  _fseeki64(pInFile, 0, SEEK_END);
  uint64 src_file_size = _ftelli64(pInFile);
  _fseeki64(pInFile, 0, SEEK_SET);

  fputc('D', pOutFile); fputc('E', pOutFile); fputc('F', pOutFile); fputc('0', pOutFile);
  fputc(options.m_write_zlib_header, pOutFile);

  for (uint i = 0; i < 8; i++)
    fputc(static_cast<int>((src_file_size >> (i * 8)) & 0xFF), pOutFile);

  uint cInBufSize = TDEFLTEST_COMP_INPUT_BUFFER_SIZE;
  uint cOutBufSize = TDEFLTEST_COMP_OUTPUT_BUFFER_SIZE;
  if (options.m_randomize_buffer_sizes)
  {
    cInBufSize = 1 + (rand() % 4096);
    cOutBufSize = 1 + (rand() % 4096);
  }
  printf("Input buffer size: %u, Output buffer size: %u\n", cInBufSize, cOutBufSize);

  uint8 *in_file_buf = static_cast<uint8*>(_aligned_malloc(cInBufSize, 16));
  uint8 *out_file_buf = static_cast<uint8*>(_aligned_malloc(cOutBufSize, 16));
  if ((!in_file_buf) || (!out_file_buf))
  {
    print_error("Out of memory!\n");
    _aligned_free(in_file_buf);
    _aligned_free(out_file_buf);
    fclose(pInFile);
    fclose(pOutFile);
    return false;
  }

  uint64 src_bytes_left = src_file_size;

  uint in_file_buf_size = 0;
  uint in_file_buf_ofs = 0;

  uint64 total_output_bytes = 0;

  timer_ticks start_time = timer::get_ticks();

  z_stream zstream;
  memset(&zstream, 0, sizeof(zstream));

  timer_ticks init_start_time = timer::get_ticks();
  int status = deflateInit2(&zstream, options.m_level, Z_DEFLATED, options.m_write_zlib_header ? Z_DEFAULT_WINDOW_BITS : -Z_DEFAULT_WINDOW_BITS, 9, options.m_z_strat);
  timer_ticks total_init_time = timer::get_ticks() - init_start_time;

  if (status != Z_OK)
  {
    print_error("Failed initializing compressor!\n");
    _aligned_free(in_file_buf);
    _aligned_free(out_file_buf);
    fclose(pInFile);
    fclose(pOutFile);
    return false;
  }

  printf("deflateInit2() took %3.3fms\n", timer::ticks_to_secs(total_init_time)*1000.0f);

  uint32 x = my_max(1, (uint32)(src_file_size ^ (src_file_size >> 32)));

  for ( ; ; )
  {
    if (src_file_size)
    {
      double total_elapsed_time = timer::ticks_to_secs(timer::get_ticks() - start_time);
      double total_bytes_processed = static_cast<double>(src_file_size - src_bytes_left);
      double comp_rate = (total_elapsed_time > 0.0f) ? total_bytes_processed / total_elapsed_time : 0.0f;

#ifdef TDEFL_PRINT_OUTPUT_PROGRESS
      for (int i = 0; i < 15; i++)
        printf("\b\b\b\b");
      printf("Progress: %3.1f%%, Bytes Remaining: %3.1fMB, %3.3fMB/sec", (1.0f - (static_cast<float>(src_bytes_left) / src_file_size)) * 100.0f, src_bytes_left / 1048576.0f, comp_rate / (1024.0f * 1024.0f));
      printf("                \b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
#endif
    }

    if (in_file_buf_ofs == in_file_buf_size)
    {
      in_file_buf_size = static_cast<uint>(my_min(cInBufSize, src_bytes_left));

      if (fread(in_file_buf, 1, in_file_buf_size, pInFile) != in_file_buf_size)
      {
        printf("\n");
        print_error("Failure reading from source file!\n");
        _aligned_free(in_file_buf);
        _aligned_free(out_file_buf);
        fclose(pInFile);
        fclose(pOutFile);
        deflateEnd(&zstream);
        return false;
      }

      src_bytes_left -= in_file_buf_size;

      in_file_buf_ofs = 0;
    }

    zstream.next_in = &in_file_buf[in_file_buf_ofs];
    zstream.avail_in = in_file_buf_size - in_file_buf_ofs;
    zstream.next_out = out_file_buf;
    zstream.avail_out = cOutBufSize;

    int flush = !src_bytes_left ? Z_FINISH : Z_NO_FLUSH;
    if ((flush == Z_NO_FLUSH) && (options.m_random_z_flushing))
    {
      RND_SHR3(x);
      if ((x & 15) == 0)
      {
        RND_SHR3(x);
        flush = (x & 31) ? Z_SYNC_FLUSH : Z_FULL_FLUSH;
      }
    }
    status = deflate(&zstream, flush);

    uint num_in_bytes = (in_file_buf_size - in_file_buf_ofs) - zstream.avail_in;
    uint num_out_bytes = cOutBufSize - zstream.avail_out;
    if (num_in_bytes)
    {
      in_file_buf_ofs += (uint)num_in_bytes;
      assert(in_file_buf_ofs <= in_file_buf_size);
    }

    if (num_out_bytes)
    {
      if (fwrite(out_file_buf, 1, static_cast<uint>(num_out_bytes), pOutFile) != num_out_bytes)
      {
        printf("\n");
        print_error("Failure writing to destination file!\n");
        _aligned_free(in_file_buf);
        _aligned_free(out_file_buf);
        fclose(pInFile);
        fclose(pOutFile);
        deflateEnd(&zstream);
        return false;
      }

      total_output_bytes += num_out_bytes;
    }

    if (status != Z_OK)
      break;
  }

#ifdef TDEFL_PRINT_OUTPUT_PROGRESS
  for (int i = 0; i < 15; i++)
  {
    printf("\b\b\b\b    \b\b\b\b");
  }
#endif

  src_bytes_left += (in_file_buf_size - in_file_buf_ofs);

  uint32 adler32 = zstream.adler;
  deflateEnd(&zstream);

  timer_ticks end_time = timer::get_ticks();
  double total_time = timer::ticks_to_secs(my_max(1, end_time - start_time));

  uint64 cmp_file_size = _ftelli64(pOutFile);

  _aligned_free(in_file_buf);
  in_file_buf = NULL;
  _aligned_free(out_file_buf);
  out_file_buf = NULL;

  fclose(pInFile);
  pInFile = NULL;
  fclose(pOutFile);
  pOutFile = NULL;

  if (status != Z_STREAM_END)
  {
    print_error("Compression failed with status %i\n", status);
    return false;
  }

  if (src_bytes_left)
  {
    print_error("Compressor failed to consume entire input file!\n");
    return false;
  }

  printf("Success\n");
  printf("Input file size: " QUAD_INT_FMT ", Compressed file size: " QUAD_INT_FMT ", Ratio: %3.2f%%\n", src_file_size, cmp_file_size, src_file_size ? ((1.0f - (static_cast<float>(cmp_file_size) / src_file_size)) * 100.0f) : 0.0f);
  printf("Compression time: %3.6f\nConsumption rate: %9.1f bytes/sec, Emission rate: %9.1f bytes/sec\n", total_time, src_file_size / total_time, cmp_file_size / total_time);
  printf("Input file adler32: 0x%08X\n", adler32);
  if (src_file_size)
  {
    if (src_file_size >= 256)
      s_max_large_comp_ratio = my_max(s_max_large_comp_ratio, cmp_file_size / (float)src_file_size);
    else
      s_max_small_comp_ratio = my_max(s_max_small_comp_ratio, cmp_file_size / (float)src_file_size);
  }
  //printf("Max small comp ratio: %f, Max large comp ratio: %f\n", s_max_small_comp_ratio, s_max_large_comp_ratio);

  return true;
}

static bool decompress_file_zlib(const char* pSrc_filename, const char *pDst_filename, comp_options options)
{
  FILE *pInFile = fopen(pSrc_filename, "rb");
  if (!pInFile)
  {
    print_error("Unable to read file: %s\n", pSrc_filename);
    return false;
  }

  _fseeki64(pInFile, 0, SEEK_END);
  uint64 src_file_size = _ftelli64(pInFile);
  _fseeki64(pInFile, 0, SEEK_SET);
  if (src_file_size < (5+9))
  {
    print_error("Compressed file is too small!\n");
    fclose(pInFile);
    return false;
  }

  int h0 = fgetc(pInFile);
  int h1 = fgetc(pInFile);
  int h2 = fgetc(pInFile);
  int h3 = fgetc(pInFile);
  int zlib_header = fgetc(pInFile);
  if ((h0 != 'D') | (h1 != 'E') || (h2 != 'F') || (h3 != '0'))
  {
    print_error("Unrecognized/invalid header in file: %s\n", pSrc_filename);
    fclose(pInFile);
    return false;
  }

  FILE *pOutFile = fopen(pDst_filename, "wb");
  if (!pOutFile)
  {
    print_error("Unable to create file: %s\n", pDst_filename);
    fclose(pInFile);
    return false;
  }

  uint64 orig_file_size = 0;
  for (uint i = 0; i < 8; i++)
    orig_file_size |= (static_cast<uint64>(fgetc(pInFile)) << (i * 8));

  int total_header_bytes = ftell(pInFile);

  // Avoid running out of memory on large files when using unbuffered decompression.
  if ((options.m_unbuffered_decompression) && (orig_file_size > 768*1024*1024))
  {
    printf("Output file is too large for unbuffered decompression - switching to streaming decompression.\n");
    options.m_unbuffered_decompression = false;
  }

  if (options.m_unbuffered_decompression)
    printf("Testing: Unbuffered decompression\n");
  else
    printf("Testing: Streaming decompression\n");

  uint cInBufSize = options.m_unbuffered_decompression ? static_cast<uint>(src_file_size) : TDEFLTEST_DECOMP_INPUT_BUFFER_SIZE;
  uint out_buf_size = options.m_unbuffered_decompression ? static_cast<uint>(orig_file_size) : TINFL_LZ_DICT_SIZE;

  if ((options.m_randomize_buffer_sizes) && (!options.m_unbuffered_decompression))
  {
    cInBufSize = 1 + (rand() % 4096);
  }

  printf("Input buffer size: %u, Output buffer size: %u\n", cInBufSize, out_buf_size);

  uint8 *in_file_buf = static_cast<uint8*>(_aligned_malloc(cInBufSize, 16));
  uint8 *out_file_buf = static_cast<uint8*>(_aligned_malloc(out_buf_size, 16));
  if ((!in_file_buf) || (!out_file_buf))
  {
    print_error("Failed allocating output buffer!\n");
    _aligned_free(in_file_buf);
    fclose(pInFile);
    fclose(pOutFile);
    return false;
  }

  uint64 src_bytes_left = src_file_size - total_header_bytes;
  uint64 dst_bytes_left = orig_file_size;

  uint in_file_buf_size = 0;
  uint in_file_buf_ofs = 0;
  uint out_file_buf_ofs = 0;

  timer_ticks start_time = timer::get_ticks();
  double decomp_only_time = 0;

  z_stream zstream;
  memset(&zstream, 0, sizeof(zstream));

  timer_ticks init_start_time = timer::get_ticks();
  int status = zlib_header ? inflateInit(&zstream) : inflateInit2(&zstream, -Z_DEFAULT_WINDOW_BITS);
  timer_ticks total_init_time = timer::get_ticks() - init_start_time;
  if (status != Z_OK)
  {
    print_error("Failed initializing decompressor!\n");
    _aligned_free(in_file_buf);
    _aligned_free(out_file_buf);
    fclose(pInFile);
    fclose(pOutFile);
    return false;
  }

  printf("inflateInit() took %3.3fms\n", timer::ticks_to_secs(total_init_time)*1000.0f);

  for ( ; ; )
  {
    if (in_file_buf_ofs == in_file_buf_size)
    {
      in_file_buf_size = static_cast<uint>(my_min(cInBufSize, src_bytes_left));

      if (fread(in_file_buf, 1, in_file_buf_size, pInFile) != in_file_buf_size)
      {
        print_error("Failure reading from source file!\n");
        _aligned_free(in_file_buf);
        _aligned_free(out_file_buf);
        deflateEnd(&zstream);
        fclose(pInFile);
        fclose(pOutFile);
        return false;
      }

      src_bytes_left -= in_file_buf_size;

      in_file_buf_ofs = 0;
    }

    uint num_in_bytes = (in_file_buf_size - in_file_buf_ofs);
    uint num_out_bytes = (out_buf_size - out_file_buf_ofs);
    zstream.next_in = in_file_buf + in_file_buf_ofs;
    zstream.avail_in = num_in_bytes;
    zstream.next_out = out_file_buf + out_file_buf_ofs;
    zstream.avail_out = num_out_bytes;

    {
      timer decomp_only_timer;
      decomp_only_timer.start();
      status = inflate(&zstream, options.m_unbuffered_decompression ? Z_FINISH : Z_SYNC_FLUSH);
      decomp_only_time += decomp_only_timer.get_elapsed_secs();
    }
    num_in_bytes -= zstream.avail_in;
    num_out_bytes -= zstream.avail_out;

    if (num_in_bytes)
    {
      in_file_buf_ofs += (uint)num_in_bytes;
      assert(in_file_buf_ofs <= in_file_buf_size);
    }

    out_file_buf_ofs += (uint)num_out_bytes;

    if ((out_file_buf_ofs == out_buf_size) || (status == Z_STREAM_END))
    {
      if (fwrite(out_file_buf, 1, static_cast<uint>(out_file_buf_ofs), pOutFile) != out_file_buf_ofs)
      {
        print_error("Failure writing to destination file!\n");
        _aligned_free(in_file_buf);
        _aligned_free(out_file_buf);
        inflateEnd(&zstream);
        fclose(pInFile);
        fclose(pOutFile);
        return false;
      }
      out_file_buf_ofs = 0;
    }

    if (num_out_bytes > dst_bytes_left)
    {
      print_error("Decompressor wrote too many bytes to destination file!\n");
      _aligned_free(in_file_buf);
      _aligned_free(out_file_buf);
      inflateEnd(&zstream);
      fclose(pInFile);
      fclose(pOutFile);
      return false;
    }
    dst_bytes_left -= num_out_bytes;

    if (status != Z_OK)
      break;
  }
  _aligned_free(in_file_buf);
  in_file_buf = NULL;

  _aligned_free(out_file_buf);
  out_file_buf = NULL;

  src_bytes_left += (in_file_buf_size - in_file_buf_ofs);

  uint32 adler32 = zstream.adler;
  inflateEnd(&zstream);

  timer_ticks end_time = timer::get_ticks();
  double total_time = timer::ticks_to_secs(my_max(1, end_time - start_time));

  fclose(pInFile);
  pInFile = NULL;

  fclose(pOutFile);
  pOutFile = NULL;

  if (status != Z_STREAM_END)
  {
    print_error("Decompression FAILED with status %i\n", status);
    return false;
  }

  if ((src_file_size < UINT_MAX) && (orig_file_size < UINT_MAX))
  {
    if ((((size_t)zstream.total_in + total_header_bytes) != src_file_size) || (zstream.total_out != orig_file_size))
    {
      print_error("Decompression FAILED to consume all input or write all expected output!\n");
      return false;
    }
  }

  if (dst_bytes_left)
  {
    print_error("Decompressor FAILED to output the entire output file!\n");
    return false;
  }

  if (src_bytes_left)
  {
    print_error("Decompressor FAILED to read " QUAD_INT_FMT " bytes from input buffer\n", src_bytes_left);
  }

  printf("Success\n");
  printf("Source file size: " QUAD_INT_FMT ", Decompressed file size: " QUAD_INT_FMT "\n", src_file_size, orig_file_size);
  if (zlib_header) printf("Decompressed adler32: 0x%08X\n", adler32);
  printf("Overall decompression time (decompression init+I/O+decompression): %3.6f\n  Consumption rate: %9.1f bytes/sec, Decompression rate: %9.1f bytes/sec\n", total_time, src_file_size / total_time, orig_file_size / total_time);
  printf("Decompression only time (not counting decompression init or I/O): %3.6f\n  Consumption rate: %9.1f bytes/sec, Decompression rate: %9.1f bytes/sec\n", decomp_only_time, src_file_size / decomp_only_time, orig_file_size / decomp_only_time);

  return true;
}

static bool compare_files(const char *pFilename1, const char* pFilename2)
{
  FILE* pFile1 = open_file_with_retries(pFilename1, "rb");
  if (!pFile1)
  {
    print_error("Failed opening file: %s\n", pFilename1);
    return false;
  }

  FILE* pFile2 = open_file_with_retries(pFilename2, "rb");
  if (!pFile2)
  {
    print_error("Failed opening file: %s\n", pFilename2);
    fclose(pFile1);
    return false;
  }

  _fseeki64(pFile1, 0, SEEK_END);
  int64 fileSize1 = _ftelli64(pFile1);
  _fseeki64(pFile1, 0, SEEK_SET);

  _fseeki64(pFile2, 0, SEEK_END);
  int64 fileSize2 = _ftelli64(pFile2);
  _fseeki64(pFile2, 0, SEEK_SET);

  if (fileSize1 != fileSize2)
  {
    print_error("Files to compare are not the same size: %I64i vs. %I64i.\n", fileSize1, fileSize2);
    fclose(pFile1);
    fclose(pFile2);
    return false;
  }

  const uint cBufSize = 1024 * 1024;
  std::vector<uint8> buf1(cBufSize);
  std::vector<uint8> buf2(cBufSize);

  int64 bytes_remaining = fileSize1;
  while (bytes_remaining)
  {
    const uint bytes_to_read = static_cast<uint>(my_min(cBufSize, bytes_remaining));

    if (fread(&buf1.front(), bytes_to_read, 1, pFile1) != 1)
    {
      print_error("Failed reading from file: %s\n", pFilename1);
      fclose(pFile1);
      fclose(pFile2);
      return false;
    }

    if (fread(&buf2.front(), bytes_to_read, 1, pFile2) != 1)
    {
      print_error("Failed reading from file: %s\n", pFilename2);
      fclose(pFile1);
      fclose(pFile2);
      return false;
    }

    if (memcmp(&buf1.front(), &buf2.front(), bytes_to_read) != 0)
    {
      print_error("File data comparison failed!\n");
      fclose(pFile1);
      fclose(pFile2);
      return false;
    }

    bytes_remaining -= bytes_to_read;
  }

  fclose(pFile1);
  fclose(pFile2);
  return true;
}

static bool zip_create(const char *pZip_filename, const char *pSrc_filename)
{
  mz_zip_archive zip;
  memset(&zip, 0, sizeof(zip));
  if ((rand() % 100) >= 10)
    zip.m_file_offset_alignment = 1 << (rand() & 15);
  if (!mz_zip_writer_init_file(&zip, pZip_filename, 65537))
  {
    print_error("Failed creating zip archive \"%s\" (1)!\n", pZip_filename);
    return false;
  }

  mz_bool success = MZ_TRUE;

  const char *pStr = "This is a test!This is a test!This is a test!\n";
  size_t comp_size;
  void *pComp_data = tdefl_compress_mem_to_heap(pStr, strlen(pStr), &comp_size, 256);
  success &= mz_zip_writer_add_mem_ex(&zip, "precomp.txt", pComp_data, comp_size, "Comment", (uint16)strlen("Comment"), MZ_ZIP_FLAG_COMPRESSED_DATA, strlen(pStr), mz_crc32(MZ_CRC32_INIT, (const uint8 *)pStr, strlen(pStr)));

  success &= mz_zip_writer_add_mem(&zip, "cool/", NULL, 0, 0);

  success &= mz_zip_writer_add_mem(&zip, "1.txt", pStr, strlen(pStr), 9);
  int n = rand() & 4095;
  for (int i = 0; i < n; i++)
  {
    char name[256], buf[256], comment[256];
    sprintf(name, "t%u.txt", i);
    sprintf(buf, "%u\n", i*5377);
    sprintf(comment, "comment: %u\n", i);
    success &= mz_zip_writer_add_mem_ex(&zip, name, buf, strlen(buf), comment, (uint16)strlen(comment), i % 10, 0, 0);
  }

  const char *pTestComment = "test comment";
  success &= mz_zip_writer_add_file(&zip, "test.bin", pSrc_filename, pTestComment, (uint16)strlen(pTestComment), 9);

  if (ensure_file_exists_and_is_readable("changelog.txt"))
    success &= mz_zip_writer_add_file(&zip, "changelog.txt", "changelog.txt", "This is a comment", (uint16)strlen("This is a comment"), 9);

  if (!success)
  {
    mz_zip_writer_end(&zip);
    remove(pZip_filename);
    print_error("Failed creating zip archive \"%s\" (2)!\n", pZip_filename);
    return false;
  }

  if (!mz_zip_writer_finalize_archive(&zip))
  {
    mz_zip_writer_end(&zip);
    remove(pZip_filename);
    print_error("Failed creating zip archive \"%s\" (3)!\n", pZip_filename);
    return false;
  }

  mz_zip_writer_end(&zip);

  struct FILE_STAT_STRUCT stat_buf;
  FILE_STAT(pZip_filename, &stat_buf);
  uint64 actual_file_size = stat_buf.st_size;
  if (zip.m_archive_size != actual_file_size)
  {
    print_error("Archive's actual size and zip archive object's size differ for file \"%s\"!\n", pZip_filename);
    return false;
  }

  printf("Created zip file \"%s\", file size: " QUAD_INT_FMT "\n", pZip_filename, zip.m_archive_size);
  return true;
}

static size_t zip_write_callback(void *pOpaque, mz_uint64 ofs, const void *pBuf, size_t n)
{
  (void)pOpaque, (void)ofs, (void)pBuf, (void)n;
  return n;
}

static bool zip_extract(const char *pZip_filename, const char *pDst_filename)
{
  mz_zip_archive zip;
  memset(&zip, 0, sizeof(zip));
  if (!mz_zip_reader_init_file(&zip, pZip_filename, 0))
  {
    print_error("Failed opening zip archive \"%s\"!\n", pZip_filename);
    return false;
  }

  int file_index = mz_zip_reader_locate_file(&zip, "test.bin", "test Comment", 0);
  int alt_file_index = mz_zip_reader_locate_file(&zip, "test.bin", "test Comment e", 0);
  if ((file_index < 0) || (alt_file_index >= 0))
  {
    print_error("Archive \"%s\" is missing test.bin file!\n", pZip_filename);
    mz_zip_reader_end(&zip);
    return false;
  }

  alt_file_index = mz_zip_reader_locate_file(&zip, "test.bin", NULL, 0);
  if (alt_file_index != file_index)
  {
    print_error("mz_zip_reader_locate_file() failed!\n", pZip_filename);
    mz_zip_reader_end(&zip);
    return false;
  }

  if (!mz_zip_reader_extract_to_file(&zip, file_index, pDst_filename, 0))
  {
    print_error("Failed extracting test.bin from archive \"%s\" err: %s!\n", pZip_filename, mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
    mz_zip_reader_end(&zip);
    return false;
  }

  for (uint i = 0; i < mz_zip_reader_get_num_files(&zip); i++)
  {
    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&zip, i, &stat))
    {
      print_error("Failed testing archive -1 \"%s\" err: %s!\n", pZip_filename, mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
      mz_zip_reader_end(&zip);
      return false;
    }
    //printf("\"%s\" %I64u %I64u\n", stat.m_filename, stat.m_comp_size, stat.m_uncomp_size);
    size_t size = 0;

    mz_bool status = mz_zip_reader_extract_to_callback(&zip, i, zip_write_callback, NULL, 0);
    if (!status)
    {
      print_error("Failed testing archive -2 \"%s\" err: %s!\n", pZip_filename, mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
      mz_zip_reader_end(&zip);
      return false;
    }

    if (stat.m_uncomp_size<100*1024*1024)
    {
        void *p = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
        if (!p)
        {
            print_error("Failed testing archive -3 \"%s\" err: %s!\n", pZip_filename, mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
            mz_zip_reader_end(&zip);
            return false;
        }
        free(p);
    }
  }
  printf("Verified %u files\n",  mz_zip_reader_get_num_files(&zip));

  mz_zip_reader_end(&zip);

  printf("Extracted file \"%s\"\n", pDst_filename);
  return true;
}

typedef std::vector< std::string > string_array;

#if defined(WIN32)
static bool find_files(std::string pathname, const std::string &filename, string_array &files, bool recursive, int depth = 0)
{
  if (!pathname.empty())
  {
    char c = pathname[pathname.size() - 1];
    if ((c != ':') && (c != '\\') && (c != '/'))
      pathname += "\\";
  }

  WIN32_FIND_DATAA find_data;

  HANDLE findHandle = FindFirstFileA((pathname + filename).c_str(), &find_data);
  if (findHandle == INVALID_HANDLE_VALUE)
  {
    HRESULT hres = GetLastError();
    if ((!depth) && (hres != NO_ERROR) && (hres != ERROR_FILE_NOT_FOUND))
      return false;
  }
  else
  {
    do
    {
      const bool is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
      const bool is_system =  (find_data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;
      const bool is_hidden =  false;//(find_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;

      std::string filename(find_data.cFileName);

      if ((!is_directory) && (!is_system) && (!is_hidden))
        files.push_back(pathname + filename);

    } while (FindNextFileA(findHandle, &find_data));

    FindClose(findHandle);
  }

  if (recursive)
  {
    string_array paths;

    HANDLE findHandle = FindFirstFileA((pathname + "*").c_str(), &find_data);
    if (findHandle != INVALID_HANDLE_VALUE)
    {
      do
      {
        const bool is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        const bool is_system =  (find_data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;
        const bool is_hidden =  false;//(find_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;

        std::string filename(find_data.cFileName);

        if ((is_directory) && (!is_hidden) && (!is_system))
          paths.push_back(filename);

      } while (FindNextFileA(findHandle, &find_data));

      FindClose(findHandle);

      for (uint i = 0; i < paths.size(); i++)
      {
        const std::string &path = paths[i];
        if (path[0] == '.')
          continue;

        if (!find_files(pathname + path, filename, files, true, depth + 1))
          return false;
      }
    }
  }

  return true;
}
#else
#include <dirent.h>
#include <fnmatch.h>
static bool find_files(std::string pathname, const std::string &pattern, string_array &files, bool recursive, int depth = 0)
{
  if (!pathname.empty())
  {
    char c = pathname[pathname.size() - 1];
    if ((c != ':') && (c != '\\') && (c != '/'))
      pathname += "/";
  }

  DIR *dp = opendir(pathname.c_str());

  if (!dp) {
    return depth ? true : false;
  }

  string_array paths;

  for ( ; ; )
  {
    struct dirent *ep = readdir(dp);
    if (!ep)
      break;

    const bool is_directory = (ep->d_type & DT_DIR) != 0;
    const bool is_file =  (ep->d_type & DT_REG) != 0;

    if (ep->d_name[0] == '.')
      continue;

    std::string filename(ep->d_name);

    if (is_directory)
    {
      if (recursive)
        paths.push_back(filename);
    }
    else if (is_file)
    {
      if (0 == fnmatch(pattern.c_str(), filename.c_str(), 0))
         files.push_back(pathname + filename);
    }
  }

  closedir(dp);
  dp = NULL;

  if (recursive)
  {
    for (uint i = 0; i < paths.size(); i++)
    {
      const std::string &path = paths[i];
      if (!find_files(pathname + path, pattern, files, true, depth + 1))
        return false;
    }
  }

  return true;
}
#endif

static bool test_recursive(const char *pPath, comp_options options)
{
  string_array files;
  if (!find_files(pPath, "*", files, true))
  {
    print_error("Failed finding files under path \"%s\"!\n", pPath);
    return false;
  }

  uint total_files_compressed = 0;
  uint64 total_source_size = 0;
  uint64 total_comp_size = 0;

#ifdef WIN32
  MEMORYSTATUS initial_mem_status;
  GlobalMemoryStatus(&initial_mem_status);
#endif

  timer_ticks start_tick_count = timer::get_ticks();

  const int first_file_index = 0;

  uint unique_id = static_cast<uint>(timer::get_init_ticks());
  char cmp_file[256], decomp_file[256];

  sprintf(cmp_file, "__comp_temp_%u__.tmp", unique_id);
  sprintf(decomp_file, "__decomp_temp_%u__.tmp", unique_id);

  for (uint file_index = first_file_index; file_index < files.size(); file_index++)
  {
    const std::string &src_file = files[file_index];

    printf("***** [%u of %u] Compressing file \"%s\" to \"%s\"\n", 1 + file_index, (uint)files.size(), src_file.c_str(), cmp_file);

    if ((strstr(src_file.c_str(), "__comp_temp") != NULL) || (strstr(src_file.c_str(), "__decomp_temp") != NULL))
    {
      printf("Skipping temporary file \"%s\"\n", src_file.c_str());
      continue;
    }

    FILE *pFile = fopen(src_file.c_str(), "rb");
    if (!pFile)
    {
      printf("Skipping unreadable file \"%s\"\n", src_file.c_str());
      continue;
    }
    _fseeki64(pFile, 0, SEEK_END);
    int64 src_file_size = _ftelli64(pFile);

    if (src_file_size)
    {
       _fseeki64(pFile, 0, SEEK_SET);
       if (fgetc(pFile) == EOF)
       {
          printf("Skipping unreadable file \"%s\"\n", src_file.c_str());
          fclose(pFile);
          continue;
       }
    }

    fclose(pFile);

    if (!ensure_file_is_writable(cmp_file))
    {
      print_error("Unable to create file \"%s\"!\n", cmp_file);
      return false;
    }

    comp_options file_options(options);
    if (options.m_randomize_params)
    {
      file_options.m_level = rand() % 11;
      file_options.m_unbuffered_decompression = (rand() & 1) != 0;
      file_options.m_z_strat = rand() % (Z_FIXED + 1);
      file_options.m_write_zlib_header = (rand() & 1) != 0;
      file_options.m_random_z_flushing = (rand() & 1) != 0;
      file_options.print();
    }

    bool status;
    if (file_options.m_archive_test)
    {
      printf("Creating test archive with file \"%s\", size " QUAD_INT_FMT "\n", src_file.c_str(), src_file_size);
      status = zip_create(cmp_file, src_file.c_str());
    }
    else
      status = compress_file_zlib(src_file.c_str(), cmp_file, file_options);
    if (!status)
    {
      print_error("Failed compressing file \"%s\" to \"%s\"\n", src_file.c_str(), cmp_file);
      return false;
    }

    if (file_options.m_verify_compressed_data)
    {
      printf("Decompressing file \"%s\" to \"%s\"\n", cmp_file, decomp_file);

      if (!ensure_file_is_writable(decomp_file))
      {
        print_error("Unable to create file \"%s\"!\n", decomp_file);
        return false;
      }

      if (file_options.m_archive_test)
        status = zip_extract(cmp_file, decomp_file);
      else
        status = decompress_file_zlib(cmp_file, decomp_file, file_options);

      if (!status)
      {
        print_error("Failed decompressing file \"%s\" to \"%s\"\n", src_file.c_str(), decomp_file);
        return false;
      }

      printf("Comparing file \"%s\" to \"%s\"\n", decomp_file, src_file.c_str());

      if (!compare_files(decomp_file, src_file.c_str()))
      {
        print_error("Failed comparing decompressed file data while compressing \"%s\" to \"%s\"\n", src_file.c_str(), cmp_file);
        return false;
      }
      else
      {
        printf("Decompressed file compared OK to original file.\n");
      }
    }

    int64 cmp_file_size = 0;
    pFile = fopen(cmp_file, "rb");
    if (pFile)
    {
      _fseeki64(pFile, 0, SEEK_END);
      cmp_file_size = _ftelli64(pFile);
      fclose(pFile);
    }

    total_files_compressed++;
    total_source_size += src_file_size;
    total_comp_size += cmp_file_size;

#ifdef WIN32
    MEMORYSTATUS mem_status;
    GlobalMemoryStatus(&mem_status);

    const int64 bytes_allocated = initial_mem_status.dwAvailVirtual- mem_status.dwAvailVirtual;

    printf("Memory allocated relative to first file: %I64i\n", bytes_allocated);
#endif

    printf("\n");
  }

  timer_ticks end_tick_count = timer::get_ticks();

  double total_elapsed_time = timer::ticks_to_secs(end_tick_count - start_tick_count);

  printf("Test successful: %f secs\n", total_elapsed_time);
  printf("Total files processed: %u\n", total_files_compressed);
  printf("Total source size: " QUAD_INT_FMT "\n", total_source_size);
  printf("Total compressed size: " QUAD_INT_FMT "\n", total_comp_size);
  printf("Max small comp ratio: %f, Max large comp ratio: %f\n", s_max_small_comp_ratio, s_max_large_comp_ratio);

  remove(cmp_file);
  remove(decomp_file);

  return true;
}

static size_t dummy_zip_file_write_callback(void *pOpaque, mz_uint64 ofs, const void *pBuf, size_t n)
{
  (void)ofs; (void)pBuf;
  uint32 *pCRC = (uint32*)pOpaque;
  *pCRC = mz_crc32(*pCRC, (const uint8*)pBuf, n);
  return n;
}

static bool test_archives(const char *pPath, comp_options options)
{
  (void)options;

  string_array files;
  if (!find_files(pPath, "*.zip", files, true))
  {
    print_error("Failed finding files under path \"%s\"!\n", pPath);
    return false;
  }

  uint total_archives = 0;
  uint64 total_bytes_processed = 0;
  uint64 total_files_processed = 0;
  uint total_errors = 0;

#ifdef WIN32
  MEMORYSTATUS initial_mem_status;
  GlobalMemoryStatus(&initial_mem_status);
#endif

  const int first_file_index = 0;
  uint unique_id = static_cast<uint>(timer::get_init_ticks());
  char cmp_file[256], decomp_file[256];

  sprintf(decomp_file, "__decomp_temp_%u__.tmp", unique_id);

  string_array failed_archives;

  for (uint file_index = first_file_index; file_index < files.size(); file_index++)
  {
    const std::string &src_file = files[file_index];

    printf("***** [%u of %u] Testing archive file \"%s\"\n", 1 + file_index, (uint)files.size(), src_file.c_str());

    if ((strstr(src_file.c_str(), "__comp_temp") != NULL) || (strstr(src_file.c_str(), "__decomp_temp") != NULL))
    {
      printf("Skipping temporary file \"%s\"\n", src_file.c_str());
      continue;
    }

    FILE *pFile = fopen(src_file.c_str(), "rb");
    if (!pFile)
    {
      printf("Skipping unreadable file \"%s\"\n", src_file.c_str());
      continue;
    }
    _fseeki64(pFile, 0, SEEK_END);
    int64 src_file_size = _ftelli64(pFile);
    fclose(pFile);

    (void)src_file_size;

    sprintf(cmp_file, "__comp_temp_%u__.zip", file_index);

    mz_zip_archive src_archive;
    memset(&src_archive, 0, sizeof(src_archive));

    if (!mz_zip_reader_init_file(&src_archive, src_file.c_str(), 0))
    {
      failed_archives.push_back(src_file);

      print_error("Failed opening archive \"%s\"!\n", src_file.c_str());
      total_errors++;
      continue;
    }

    mz_zip_archive dst_archive;
    memset(&dst_archive, 0, sizeof(dst_archive));
    if (options.m_write_archives)
    {
      if (!ensure_file_is_writable(cmp_file))
      {
        print_error("Unable to create file \"%s\"!\n", cmp_file);
        return false;
      }

      if (!mz_zip_writer_init_file(&dst_archive, cmp_file, 0))
      {
        print_error("Failed creating archive \"%s\"!\n", cmp_file);
        total_errors++;
        continue;
      }
    }

    int i;
    //for (i = 0; i < mz_zip_reader_get_num_files(&src_archive); i++)
    for (i = mz_zip_reader_get_num_files(&src_archive) - 1; i >= 0; --i)
    {
      if (mz_zip_reader_is_file_encrypted(&src_archive, i))
        continue;

      mz_zip_archive_file_stat file_stat;
      bool status = mz_zip_reader_file_stat(&src_archive, i, &file_stat) != 0;

      int locate_file_index = mz_zip_reader_locate_file(&src_archive, file_stat.m_filename, NULL, 0);
      if (locate_file_index != i)
      {
        mz_zip_archive_file_stat locate_file_stat;
        mz_zip_reader_file_stat(&src_archive, locate_file_index, &locate_file_stat);
        if (_stricmp(locate_file_stat.m_filename, file_stat.m_filename) != 0)
        {
          print_error("mz_zip_reader_locate_file() failed!\n");
          return false;
        }
        else
        {
          printf("Warning: Duplicate filenames in archive!\n");
        }
      }

      if ((file_stat.m_method) && (file_stat.m_method != MZ_DEFLATED))
        continue;

      if (status)
      {
        char name[260];
        mz_zip_reader_get_filename(&src_archive, i, name, sizeof(name));

        size_t extracted_size = 0;
        void *p = mz_zip_reader_extract_file_to_heap(&src_archive, name, &extracted_size, 0);
        if (!p)
          status = false;

        uint32 extracted_crc32 = MZ_CRC32_INIT;
        if (!mz_zip_reader_extract_file_to_callback(&src_archive, name, dummy_zip_file_write_callback, &extracted_crc32, 0))
          status = false;

        if (mz_crc32(MZ_CRC32_INIT, (const uint8*)p, extracted_size) != extracted_crc32)
          status = false;

        free(p);

        if (options.m_write_archives)
        {
          if ((status) && (!mz_zip_writer_add_from_zip_reader(&dst_archive, &src_archive, i)))
          {
            print_error("Failed adding new file to archive \"%s\"!\n", cmp_file);
            status = false;
          }
        }

        total_bytes_processed += file_stat.m_uncomp_size;
        total_files_processed++;
      }

      if (!status)
        break;
    }

    mz_zip_reader_end(&src_archive);

    //if (i < mz_zip_reader_get_num_files(&src_archive))
    if (i >= 0)
    {
      failed_archives.push_back(src_file);

      print_error("Failed processing archive \"%s\"!\n", src_file.c_str());
      total_errors++;
    }

    if (options.m_write_archives)
    {
      if (!mz_zip_writer_finalize_archive(&dst_archive) || !mz_zip_writer_end(&dst_archive))
      {
        failed_archives.push_back(src_file);

        print_error("Failed finalizing archive \"%s\"!\n", cmp_file);
        total_errors++;
      }
    }

    total_archives++;

#ifdef WIN32
    MEMORYSTATUS mem_status;
    GlobalMemoryStatus(&mem_status);

    const int64 bytes_allocated = initial_mem_status.dwAvailVirtual- mem_status.dwAvailVirtual;

    printf("Memory allocated relative to first file: %I64i\n", bytes_allocated);
#endif

    printf("\n");
  }

  printf("Total archives processed: %u\n", total_archives);
  printf("Total errors: %u\n", total_errors);
  printf("Total bytes processed: " QUAD_INT_FMT "\n", total_bytes_processed);
  printf("Total archive files processed: " QUAD_INT_FMT "\n", total_files_processed);

  printf("List of failed archives:\n");
  for (uint i = 0; i < failed_archives.size(); ++i)
    printf("%s\n", failed_archives[i].c_str());

  remove(cmp_file);
  remove(decomp_file);

  return true;
}

int main_internal(string_array cmd_line)
{
  comp_options options;

  if (!cmd_line.size())
  {
    print_usage();
    if (simple_test1(options) || simple_test2(options))
      return EXIT_FAILURE;
    return EXIT_SUCCESS;
  }

  enum op_mode_t
  {
    OP_MODE_INVALID = -1,
    OP_MODE_COMPRESS = 0,
    OP_MODE_DECOMPRESS = 1,
    OP_MODE_ALL = 2,
    OP_MODE_ARCHIVES = 3
  };

  op_mode_t op_mode = OP_MODE_INVALID;

  for (int i = 0; i < (int)cmd_line.size(); i++)
  {
    const std::string &str = cmd_line[i];
    if (str[0] == '-')
    {
      if (str.size() < 2)
      {
        print_error("Invalid option: %s\n", str.c_str());
        return EXIT_FAILURE;
      }
      switch (tolower(str[1]))
      {
        case 'u':
        {
          options.m_unbuffered_decompression = true;
          break;
        }
        case 'm':
        {
          int comp_level = atoi(str.c_str() + 2);
          if ((comp_level < 0) || (comp_level > (int)10))
          {
            print_error("Invalid compression level: %s\n", str.c_str());
            return EXIT_FAILURE;
          }

          options.m_level = comp_level;
          break;
        }
        case 'v':
        {
          options.m_verify_compressed_data = true;
          break;
        }
        case 'r':
        {
          options.m_randomize_params = true;
          break;
        }
        case 'b':
        {
          options.m_randomize_buffer_sizes = true;
          break;
        }
        case 'h':
        {
          options.m_random_z_flushing = true;
          break;
        }
        case 'x':
        {
          int seed = atoi(str.c_str() + 2);
          srand(seed);
          printf("Using random seed: %i\n", seed);
          break;
        }
        case 't':
        {
          options.m_z_strat = my_min(Z_FIXED, my_max(0, atoi(str.c_str() + 2)));
          break;
        }
        case 'z':
        {
          options.m_write_zlib_header = false;
          break;
        }
        case 'a':
        {
          options.m_archive_test = true;
          break;
        }
        case 'w':
        {
          options.m_write_archives = true;
          break;
        }
        default:
        {
          print_error("Invalid option: %s\n", str.c_str());
          return EXIT_FAILURE;
        }
      }

      cmd_line.erase(cmd_line.begin() + i);
      i--;

      continue;
    }

    if (str.size() != 1)
    {
      print_error("Invalid mode: %s\n", str.c_str());
      return EXIT_FAILURE;
    }
    switch (tolower(str[0]))
    {
      case 'c':
      {
        op_mode = OP_MODE_COMPRESS;
        break;
      }
      case 'd':
      {
        op_mode = OP_MODE_DECOMPRESS;
        break;
      }
      case 'a':
      {
        op_mode = OP_MODE_ALL;
        break;
      }
      case 'r':
      {
        op_mode = OP_MODE_ARCHIVES;
        break;
      }
      default:
      {
        print_error("Invalid mode: %s\n", str.c_str());
        return EXIT_FAILURE;
      }
    }
    cmd_line.erase(cmd_line.begin() + i);
    break;
  }

  if (op_mode == OP_MODE_INVALID)
  {
    print_error("No mode specified!\n");
    print_usage();
    return EXIT_FAILURE;
  }

  printf("Using options:\n");
  options.print();
  printf("\n");

  int exit_status = EXIT_FAILURE;

  switch (op_mode)
  {
    case OP_MODE_COMPRESS:
    {
      if (cmd_line.size() < 2)
      {
        print_error("Must specify input and output filenames!\n");
        return EXIT_FAILURE;
      }
      else if (cmd_line.size() > 2)
      {
        print_error("Too many filenames!\n");
        return EXIT_FAILURE;
      }

      const std::string &src_file = cmd_line[0];
      const std::string &cmp_file = cmd_line[1];

      bool comp_result = compress_file_zlib(src_file.c_str(), cmp_file.c_str(), options);
      if (comp_result)
        exit_status = EXIT_SUCCESS;

      if ((comp_result) && (options.m_verify_compressed_data))
      {
        char decomp_file[256];

        sprintf(decomp_file, "__decomp_temp_%u__.tmp", (uint)timer::get_ms());

        if (!decompress_file_zlib(cmp_file.c_str(), decomp_file, options))
        {
          print_error("Failed decompressing file \"%s\" to \"%s\"\n", cmp_file.c_str(), decomp_file);
          return EXIT_FAILURE;
        }

        printf("Comparing file \"%s\" to \"%s\"\n", decomp_file, src_file.c_str());

        if (!compare_files(decomp_file, src_file.c_str()))
        {
          print_error("Failed comparing decompressed file data while compressing \"%s\" to \"%s\"\n", src_file.c_str(), cmp_file.c_str());
          return EXIT_FAILURE;
        }
        else
        {
          printf("Decompressed file compared OK to original file.\n");
        }

        remove(decomp_file);
      }

      break;
    }
    case OP_MODE_DECOMPRESS:
    {
      if (cmd_line.size() < 2)
      {
        print_error("Must specify input and output filenames!\n");
        return EXIT_FAILURE;
      }
      else if (cmd_line.size() > 2)
      {
        print_error("Too many filenames!\n");
        return EXIT_FAILURE;
      }
      if (decompress_file_zlib(cmd_line[0].c_str(), cmd_line[1].c_str(), options))
        exit_status = EXIT_SUCCESS;
      break;
    }
    case OP_MODE_ALL:
    {
      if (!cmd_line.size())
      {
        print_error("No directory specified!\n");
        return EXIT_FAILURE;
      }
      else if (cmd_line.size() != 1)
      {
        print_error("Too many filenames!\n");
        return EXIT_FAILURE;
      }
      if (test_recursive(cmd_line[0].c_str(), options))
        exit_status = EXIT_SUCCESS;
      break;
    }
    case OP_MODE_ARCHIVES:
    {
      if (!cmd_line.size())
      {
        print_error("No directory specified!\n");
        return EXIT_FAILURE;
      }
      else if (cmd_line.size() != 1)
      {
        print_error("Too many filenames!\n");
        return EXIT_FAILURE;
      }
      if (test_archives(cmd_line[0].c_str(), options))
        exit_status = EXIT_SUCCESS;
      break;
    }
    default:
    {
      print_error("No mode specified!\n");
      print_usage();
      return EXIT_FAILURE;
    }
  }

  return exit_status;
}

int main(int argc, char *argv[])
{
#if defined(_WIN64) || defined(__LP64__) || defined(_LP64)
  printf("miniz.c x64 Command Line Test App - Compiled %s %s\n", __DATE__, __TIME__);
#else
  printf("miniz.c x86 Command Line Test App - Compiled %s %s\n", __DATE__, __TIME__);
#endif
  timer::get_ticks();

  string_array cmd_line;
  for (int i = 1; i < argc; i++)
    cmd_line.push_back(std::string(argv[i]));

  int exit_status = main_internal(cmd_line);

  return exit_status;
}
