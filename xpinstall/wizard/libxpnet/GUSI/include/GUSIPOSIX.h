// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIPOSIX.nw		-	POSIX/Socket wrappers                         
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIPOSIX.h,v $
// % Revision 1.1  2001/03/11 22:37:38  sgehani%netscape.com
// % First Checked In.
// %                                                
// % Revision 1.22  2001/01/17 08:58:06  neeri                             
// % Releasing 2.1.4                                                       
// %                                                                       
// % Revision 1.21  2000/10/29 18:36:32  neeri                             
// % Fix time_t signedness issues                                          
// %                                                                       
// % Revision 1.20  2000/10/16 04:34:23  neeri                             
// % Releasing 2.1.2                                                       
// %                                                                       
// % Revision 1.19  2000/06/12 04:24:50  neeri                             
// % Fix time, localtime, gmtime                                           
// %                                                                       
// % Revision 1.18  2000/05/23 07:15:30  neeri                             
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.17  2000/03/06 08:18:25  neeri                             
// % Fix sleep, usleep, chdir; new Yield system                            
// %                                                                       
// % Revision 1.16  2000/01/17 01:41:21  neeri                             
// % Fix rename() mangling                                                 
// %                                                                       
// % Revision 1.15  1999/12/13 03:01:48  neeri                             
// % Another select() fix                                                  
// %                                                                       
// % Revision 1.14  1999/11/15 07:22:34  neeri                             
// % Safe context setup. Fix sleep checking.                               
// %                                                                       
// % Revision 1.13  1999/09/09 07:21:22  neeri                             
// % Add support for inet_aton                                             
// %                                                                       
// % Revision 1.12  1999/08/26 05:45:06  neeri                             
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.11  1999/07/19 06:21:03  neeri                             
// % Add mkdir/rmdir, fix various file manager related bugs                
// %                                                                       
// % Revision 1.10  1999/07/07 04:17:42  neeri                             
// % Final tweaks for 2.0b3                                                
// %                                                                       
// % Revision 1.9  1999/06/28 06:04:59  neeri                              
// % Support interrupted calls                                             
// %                                                                       
// % Revision 1.8  1999/05/30 03:09:31  neeri                              
// % Added support for MPW compilers                                       
// %                                                                       
// % Revision 1.7  1999/04/29 05:33:18  neeri                              
// % Fix fcntl prototype                                                   
// %                                                                       
// % Revision 1.6  1999/03/29 09:51:29  neeri                              
// % New configuration system with support for hardcoded configurations.   
// %                                                                       
// % Revision 1.5  1999/03/17 09:05:11  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.4  1998/10/25 11:35:19  neeri                              
// % chdir, getcwd, setxxxent                                              
// %                                                                       
// % Revision 1.3  1998/10/11 16:45:22  neeri                              
// % Ready to release 2.0a2                                                
// %                                                                       
// % Revision 1.2  1998/08/01 21:32:09  neeri                              
// % About ready for 2.0a1                                                 
// %                                                                       
// % Revision 1.1  1998/01/25 21:02:49  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{POSIX/Socket Wrappers}                                         
//                                                                         
// Now everything is in place to define the POSIX and socket routines      
// themselves. As opposed to our usual practice, we don't declare the      
// exported routines here, as they all have been declared in [[unistd.h]]  
// or [[sys/socket.h]] already. The exceptions are [[remove]] and [[rename]], 
// which are declared in [[stdio.h]], which we'd rather not include, and   
// various calls which are not consistently declared.                      
//                                                                         
// <GUSIPOSIX.h>=                                                          
#ifndef _GUSIPOSIX_
#define _GUSIPOSIX_

#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <utime.h>
#include <netdb.h>
#include <arpa/inet.h>

__BEGIN_DECLS
int remove(const char * path);
int rename(const char *oldname, const char *newname);
int fgetfileinfo(const char * path, OSType * creator, OSType * type);
void fsetfileinfo(const char * path, OSType creator, OSType type);
time_t time(time_t * timer);
struct tm * localtime(const time_t * timer);
struct tm * gmtime(const time_t * timer);
time_t mktime(struct tm *timeptr);
__END_DECLS

#endif /* _GUSIPOSIX_ */
