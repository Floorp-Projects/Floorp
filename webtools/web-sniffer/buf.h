#ifndef _BUF_H_
#define _BUF_H_

typedef struct Buf Buf;

Buf *bufAlloc(int fd);
unsigned char *bufCopy(Buf *buf);
unsigned char *bufCopyLower(Buf *buf);
unsigned char *bufCopyMemory(Buf *buf, unsigned long *len);
unsigned long bufCurrent(Buf *buf);
int bufError(Buf *buf);
void bufFree(Buf *buf);
unsigned short bufGetByte(Buf *buf);
void bufMark(Buf *buf, int offset);
void bufPutChar(Buf *buf, unsigned char c);
void bufPutString(Buf *buf, unsigned char *str);
void bufSend(Buf *buf);
void bufSet(Buf *buf, unsigned long offset);
void bufSetFD(Buf *buf, int fd);
unsigned short bufTrimTrailingWhiteSpace(Buf *buf);
void bufUnGetByte(Buf *buf);
void bufWrite(Buf *buf);

#endif /* _BUF_H_ */
