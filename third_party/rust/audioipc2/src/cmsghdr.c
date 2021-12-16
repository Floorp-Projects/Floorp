#include <sys/socket.h>
#include <inttypes.h>
#include <string.h>

const uint8_t*
cmsghdr_bytes(size_t* size)
{
  int myfd = 0;

  static union {
    uint8_t buf[CMSG_SPACE(sizeof(myfd))];
    struct cmsghdr align;
  } u;

  u.align.cmsg_len = CMSG_LEN(sizeof(myfd));
  u.align.cmsg_level = SOL_SOCKET;
  u.align.cmsg_type = SCM_RIGHTS;

  memcpy(CMSG_DATA(&u.align), &myfd, sizeof(myfd));

  *size = sizeof(u);
  return (const uint8_t*)&u.buf;
}
