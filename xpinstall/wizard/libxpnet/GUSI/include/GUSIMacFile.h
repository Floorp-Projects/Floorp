// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIMacFile.nw		-	Disk files                                  
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIMacFile.h,v $
// % Revision 1.1  2001/03/11 22:35:45  sgehani%netscape.com
// % First Checked In.
// %                                              
// % Revision 1.24  2001/01/17 08:58:06  neeri                             
// % Releasing 2.1.4                                                       
// %                                                                       
// % Revision 1.23  2000/12/23 06:11:36  neeri                             
// % Move diagnostics to A5 safe time                                      
// %                                                                       
// % Revision 1.22  2000/10/16 04:02:00  neeri                             
// % Save A5 in completion routines                                        
// %                                                                       
// % Revision 1.21  2000/05/23 07:07:05  neeri                             
// % Improve formatting, fix lseek for readonly files                      
// %                                                                       
// % Revision 1.20  2000/03/15 07:17:11  neeri                             
// % Fix rename for existing targets                                       
// %                                                                       
// % Revision 1.19  2000/03/06 08:12:27  neeri                             
// % New Yield system; fix readdir at end of directory                     
// %                                                                       
// % Revision 1.18  1999/11/15 07:24:32  neeri                             
// % Return error for stat() of nonexistent files.                         
// %                                                                       
// % Revision 1.17  1999/09/09 07:19:19  neeri                             
// % Fix read-ahead switch-off                                             
// %                                                                       
// % Revision 1.16  1999/08/26 05:45:05  neeri                             
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.15  1999/08/05 05:55:35  neeri                             
// % Updated for CW Pro 5                                                  
// %                                                                       
// % Revision 1.14  1999/08/02 07:02:45  neeri                             
// % Support for asynchronous errors and other socket options              
// %                                                                       
// % Revision 1.13  1999/07/19 06:21:03  neeri                             
// % Add mkdir/rmdir, fix various file manager related bugs                
// %                                                                       
// % Revision 1.12  1999/07/07 04:17:41  neeri                             
// % Final tweaks for 2.0b3                                                
// %                                                                       
// % Revision 1.11  1999/06/28 06:07:15  neeri                             
// % Support I/O alignment, more effective writeback strategy              
// %                                                                       
// % Revision 1.10  1999/05/30 02:18:05  neeri                             
// % Cleaner definition of GUSICatInfo                                     
// %                                                                       
// % Revision 1.9  1999/04/29 05:33:19  neeri                              
// % Fix fcntl prototype                                                   
// %                                                                       
// % Revision 1.8  1999/04/10 04:54:39  neeri                              
// % stat() was broken for directories                                     
// %                                                                       
// % Revision 1.7  1999/03/29 09:51:28  neeri                              
// % New configuration system with support for hardcoded configurations.   
// %                                                                       
// % Revision 1.6  1999/03/17 09:05:09  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.5  1998/11/22 23:06:57  neeri                              
// % Releasing 2.0a4 in a hurry                                            
// %                                                                       
// % Revision 1.4  1998/10/11 16:45:19  neeri                              
// % Ready to release 2.0a2                                                
// %                                                                       
// % Revision 1.3  1998/08/01 21:28:58  neeri                              
// % Add directory operations                                              
// %                                                                       
// % Revision 1.2  1998/02/11 12:57:14  neeri                              
// % PowerPC Build                                                         
// %                                                                       
// % Revision 1.1  1998/01/25 21:02:48  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Disk files}                                                    
//                                                                         
// A [[GUSIMacFileSocket]] implements the operations on mac files. All instances
// of [[GUSIMacFileSocket]] are created by the [[GUSIMacFileDevice]] singleton, so
// there is no point in exporting the class itself.                        
//                                                                         
// A [[GUSIMacDirectory]] implements directory handles on mac directories. 
//                                                                         
// <GUSIMacFile.h>=                                                        
#ifndef _GUSIMacFile_
#define _GUSIMacFile_

#ifdef GUSI_INTERNAL

#include "GUSIDevice.h"

// \section{Definition of [[GUSIMacFileDevice]]}                           
//                                                                         
// [[GUSIMacFileDevice]] is a singleton subclass of [[GUSIDevice]].        
//                                                                         
// <Definition of class [[GUSIMacFileDevice]]>=                            
class GUSIMacFileDevice : public GUSIDevice {
public:
	static GUSIMacFileDevice *	Instance();
	virtual bool	Want(GUSIFileToken & file);
	
	// <Overridden member functions for [[GUSIMacFileDevice]]>=                
 virtual GUSISocket * open(GUSIFileToken & file, int flags);
 // The normal case of [[remove]] is straightforward, but we also want to support
 // the removing of open files, which is frequently used in POSIX code, as much
 // as possible.                                                            
 //                                                                         
 // <Overridden member functions for [[GUSIMacFileDevice]]>=                
 virtual int remove(GUSIFileToken & file);
 // [[rename]] can be a surprisingly difficult operation.                   
 //                                                                         
 // <Overridden member functions for [[GUSIMacFileDevice]]>=                
 virtual int rename(GUSIFileToken & file, const char * newname);
 // [[stat]] is a rather intimidating function which needs to pull together 
 // information from various sources.                                       
 //                                                                         
 // <Overridden member functions for [[GUSIMacFileDevice]]>=                
 virtual int stat(GUSIFileToken & file, struct stat * buf);
 // <Overridden member functions for [[GUSIMacFileDevice]]>=                
 virtual int chmod(GUSIFileToken & file, mode_t mode);
 // <Overridden member functions for [[GUSIMacFileDevice]]>=                
 virtual int utime(GUSIFileToken & file, const utimbuf * times);
 // <Overridden member functions for [[GUSIMacFileDevice]]>=                
 virtual int access(GUSIFileToken & file, int mode);
 // <Overridden member functions for [[GUSIMacFileDevice]]>=                
 virtual int mkdir(GUSIFileToken & file);
 // <Overridden member functions for [[GUSIMacFileDevice]]>=                
 virtual int rmdir(GUSIFileToken & file);
 // <Overridden member functions for [[GUSIMacFileDevice]]>=                
 virtual GUSIDirectory * opendir(GUSIFileToken & file);
 // [[symlink]] has to reproduce the Finder's alias creation process, which is
 // quite complex.                                                          
 //                                                                         
 // <Overridden member functions for [[GUSIMacFileDevice]]>=                
 virtual int symlink(GUSIFileToken & to, const char * newlink);
 // <Overridden member functions for [[GUSIMacFileDevice]]>=                
 virtual int readlink(GUSIFileToken & link, char * buf, int bufsize);
 // <Overridden member functions for [[GUSIMacFileDevice]]>=                
 virtual int fgetfileinfo(GUSIFileToken & file, OSType * creator, OSType * type);
 virtual int fsetfileinfo(GUSIFileToken & file, OSType creator, OSType type);
 // [[faccess]] is a somewhat curious case in that [[GUSIMacFileDevice]]    
 // accepts responsibility for handling it, but then does not, in fact, handle
 // it.                                                                     
 //                                                                         
 // <Overridden member functions for [[GUSIMacFileDevice]]>=                
 virtual int faccess(GUSIFileToken & file, unsigned * cmd, void * arg);
	
	GUSISocket * 	open(short fileRef, int flags);
	
	int 			MarkTemporary(const FSSpec & file);
	void			CleanupTemporaries(bool giveup);
	
	~GUSIMacFileDevice();
protected:
	GUSIMacFileDevice()	: fTemporaries(0)		{}

	// [[MarkTemporary]] moves the file to the temporary folder and puts it on a list 
 // of death candidates.                                                    
 //                                                                         
 // <Privatissima of [[GUSIMacFileDevice]]>=                                
 struct TempQueue {
 	TempQueue *	fNext;
 	FSSpec		fSpec;
 } * fTemporaries;
};

#endif /* GUSI_INTERNAL */

#endif /* _GUSIMacFile_ */
