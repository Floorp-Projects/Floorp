


#include "xp_mcom.h"
#include "net.h"
#include "xp_linebuf.h"
#include "mkbuf.h"

#ifndef MOZ_USER_DIR
#define MOZ_USER_DIR ".mozilla"
#endif

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

extern "C" char *fe_GetConfigDir(void) {
  char *home = getenv("HOME");
  if (home) {
    int len = strlen(home);
    len += strlen("/") + strlen(MOZ_USER_DIR) + 1;

    char* config_dir = (char *)XP_CALLOC(len, sizeof(char));
    // we really should use XP_STRN*_SAFE but this is MODULAR_NETLIB
    XP_STRCPY(config_dir, home);
    XP_STRCAT(config_dir, "/");
    XP_STRCAT(config_dir, MOZ_USER_DIR); 
    return config_dir;
  }
  
  return strdup("/tmp");
}
