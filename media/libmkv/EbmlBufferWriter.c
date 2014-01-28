// #include <strmif.h>
#include "EbmlBufferWriter.h"
#include "EbmlWriter.h"
// #include <cassert>
// #include <limits>
// #include <malloc.h>  //_alloca
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

void
Ebml_Serialize(EbmlGlobal *glob, const void *buffer_in, int buffer_size, unsigned long len)
{
  /* buffer_size:
   * 1 - int8_t;
   * 2 - int16_t;
   * 3 - int32_t;
   * 4 - int64_t;
   */
  long i;
  for(i = len-1; i >= 0; i--) {
    unsigned char x;
    if (buffer_size == 1) {
      x = (char)(*(const int8_t *)buffer_in >> (i * 8));
	} else if (buffer_size == 2) {
      x = (char)(*(const int16_t *)buffer_in >> (i * 8));
	} else if (buffer_size == 4) {
      x = (char)(*(const int32_t *)buffer_in >> (i * 8));
	} else if (buffer_size == 8) {
      x = (char)(*(const int64_t *)buffer_in >> (i * 8));
	}
    Ebml_Write(glob, &x, 1);
  }
}

void Ebml_Write(EbmlGlobal *glob, const void *buffer_in, unsigned long len) {
  unsigned char *src = glob->buf;
  src += glob->offset;
  memcpy(src, buffer_in, len);
  glob->offset += len;
}

static void _Serialize(EbmlGlobal *glob, const unsigned char *p, const unsigned char *q) {
  while (q != p) {
    --q;

    memcpy(&(glob->buf[glob->offset]), q, 1);
    glob->offset++;
  }
}

/*
void Ebml_Serialize(EbmlGlobal *glob, const void *buffer_in, unsigned long len) {
  // assert(buf);

  const unsigned char *const p = (const unsigned char *)(buffer_in);
  const unsigned char *const q = p + len;

  _Serialize(glob, p, q);
}
*/

void Ebml_StartSubElement(EbmlGlobal *glob, EbmlLoc *ebmlLoc, unsigned long class_id) {
  unsigned long long unknownLen = 0x01FFFFFFFFFFFFFFLL;
  Ebml_WriteID(glob, class_id);
  ebmlLoc->offset = glob->offset;
  // todo this is always taking 8 bytes, this may need later optimization
  Ebml_Serialize(glob, (void *)&unknownLen,sizeof(unknownLen), 8); // this is a key that says lenght unknown
}

void Ebml_EndSubElement(EbmlGlobal *glob, EbmlLoc *ebmlLoc) {
  unsigned long long size = glob->offset - ebmlLoc->offset - 8;
  unsigned long long curOffset = glob->offset;
  glob->offset = ebmlLoc->offset;
  size |=  0x0100000000000000LL;
  Ebml_Serialize(glob, &size,sizeof(size), 8);
  glob->offset = curOffset;
}

