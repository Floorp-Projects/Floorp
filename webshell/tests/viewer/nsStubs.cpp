


#include "xp_mcom.h"
#include "net.h"
#include "xp_linebuf.h"
#include "mkbuf.h"

extern "C" XP_Bool ValidateDocData(MWContext *window_id) 
{ 
  printf("ValidateDocData not implemented, stubbed in webshell/tests/viewer/nsStubs.cpp\n"); 
  return PR_TRUE; 
}

/* dist/public/xp/xp_linebuf.h */
extern "C" int XP_ReBuffer (const char *net_buffer, int32 net_buffer_size,
                        uint32 desired_buffer_size,
                        char **bufferP, uint32 *buffer_sizeP,
                        uint32 *buffer_fpP,
                        int32 (*per_buffer_fn) (char *buffer,
                                                uint32 buffer_size,
                                                void *closure),
                        void *closure) 
{ 

  printf("XP_ReBuffer not implemented, stubbed in webshell/tests/viewer/nsStubs.cpp\n"); 
  return(0); 
}



/* mozilla/include/xp_trace.h */

extern "C" void XP_Trace( const char *, ... ) 
{ 
  printf("XP_Trace not implemented, stubbed in webshell/tests/viewer/nsStubs.cpp\n"); 


} 


