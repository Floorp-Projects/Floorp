#ifndef EBMLBUFFERWRITER_HPP
#define EBMLBUFFERWRITER_HPP

typedef struct {
  unsigned long long offset;
} EbmlLoc;

typedef struct {
  unsigned char *buf;
  unsigned int length;
  unsigned int offset;
} EbmlGlobal;

void Ebml_Write(EbmlGlobal *glob, const void *buffer_in, unsigned long len);
void Ebml_Serialize(EbmlGlobal *glob, const void *buffer_in,
                    int buffer_size, unsigned long len);
void Ebml_StartSubElement(EbmlGlobal *glob, EbmlLoc *ebmlLoc, unsigned long class_id);
void Ebml_EndSubElement(EbmlGlobal *glob,  EbmlLoc *ebmlLoc);

#endif
