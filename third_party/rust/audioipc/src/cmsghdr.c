#include <sys/socket.h>
#include <inttypes.h>
#include <string.h>

const uint8_t*
cmsghdr_bytes(size_t* size)
{
  int myfds[3] = {0, 1, 2};

  static union {
    uint8_t buf[CMSG_SPACE(sizeof(myfds))];
    struct cmsghdr align;
  } u;

  u.align.cmsg_len = CMSG_LEN(sizeof(myfds));
  u.align.cmsg_level = SOL_SOCKET;
  u.align.cmsg_type = SCM_RIGHTS;

  memcpy(CMSG_DATA(&u.align), myfds, sizeof(myfds));

  *size = sizeof(u);
  return (const uint8_t*)&u.buf;
}
