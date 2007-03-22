// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIDevice.nw		-	Devices                                      
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIDevice.h,v $
// % Revision 1.1  2001/03/11 22:34:48  sgehani%netscape.com
// % First Checked In.
// %                                               
// % Revision 1.13  2000/06/12 04:22:30  neeri                             
// % Return values, not references                                         
// %                                                                       
// % Revision 1.12  2000/05/23 06:58:03  neeri                             
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.11  2000/03/15 07:22:06  neeri                             
// % Enforce alignment choices                                             
// %                                                                       
// % Revision 1.10  2000/03/06 06:30:30  neeri                             
// % Check for nonexistent device                                          
// %                                                                       
// % Revision 1.9  1999/08/26 05:45:01  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.8  1999/07/19 06:21:02  neeri                              
// % Add mkdir/rmdir, fix various file manager related bugs                
// %                                                                       
// % Revision 1.7  1999/05/29 06:26:42  neeri                              
// % Fixed header guards                                                   
// %                                                                       
// % Revision 1.6  1999/03/17 09:05:07  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.5  1998/11/22 23:06:52  neeri                              
// % Releasing 2.0a4 in a hurry                                            
// %                                                                       
// % Revision 1.4  1998/10/25 11:37:38  neeri                              
// % More configuration hooks                                              
// %                                                                       
// % Revision 1.3  1998/10/11 16:45:13  neeri                              
// % Ready to release 2.0a2                                                
// %                                                                       
// % Revision 1.2  1998/08/01 21:28:57  neeri                              
// % Add directory operations                                              
// %                                                                       
// % Revision 1.1  1998/01/25 21:02:45  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// % Revision 1.1  1996/12/16 02:12:40  neeri                              
// % TCP Sockets sort of work                                              
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Devices}                                                       
//                                                                         
// Similar to the creation of sockets, operations on files like opening or 
// renaming them need to be dispatched to a variety of special cases (Most of
// them of the form "Dev:" preceded by a device name). Analogous to the    
// [[GUSISocketFactory]] subclasses registered in a [[GUSISocketDomainRegistry]],
// we therefore have subclasses of [[GUSIDevice]] registered in a          
// [[GUSIDeviceRegistry]], although the details of the two registries are  
// quite different.                                                        
//                                                                         
// During resolution of a file name, the name and information about it is passed
// around in a [[GUSIFileToken]].                                          
//                                                                         
// <GUSIDevice.h>=                                                         
#ifndef _GUSIDevice_
#define _GUSIDevice_

#ifdef GUSI_SOURCE

#include "GUSISocket.h"
#include "GUSIFileSpec.h"

#include <dirent.h>
#include <utime.h>

#include <ConditionalMacros.h>

#if PRAGMA_STRUCT_ALIGN
#pragma options align=native
#endif

// \section{Definition of [[GUSIFileToken]]}                               
//                                                                         
// A [[GUSIFileToken]] consists of a pointer to the name as a C string, of a pointer
// to the [[GUSIDevice]] the token resolves to, and, if the token refers to a
// file name rather than a device name, a pointer to a [[GUSIFileSpec]]. Since 
// depending on the call, different [[GUSIDevice]] subclasses may handle it, a
// request code has to be passed to the constructor, too.                  
//                                                                         
// <Definition of class [[GUSIFileToken]]>=                                
class GUSIDevice;

class GUSIFileToken : public GUSIFileSpec {
public:
	enum Request {
		// \section{Operations on Devices}                                         
  //                                                                         
  // The [[open]] operation creates a new socket for the specified path or file
  // specification.                                                          
  //                                                                         
  // <Requests for [[GUSIFileToken]]>=                                       
  kWillOpen,
  // [[remove]] deletes a path or file specification.                        
  //                                                                         
  // <Requests for [[GUSIFileToken]]>=                                       
  kWillRemove,
  // [[rename]] renames a path or file specification.                        
  //                                                                         
  // <Requests for [[GUSIFileToken]]>=                                       
  kWillRename,
  // [[stat]] gathers statistical data about a file or directory.            
  //                                                                         
  // <Requests for [[GUSIFileToken]]>=                                       
  kWillStat,
  // [[chmod]] changes file modes, to the extent that this is meaningful on MacOS.
  //                                                                         
  // <Requests for [[GUSIFileToken]]>=                                       
  kWillChmod,
  // [[utime]] bumps a file's modification time.                             
  //                                                                         
  // <Requests for [[GUSIFileToken]]>=                                       
  kWillUtime,
  // [[access]] checks access permissions for a file.                        
  //                                                                         
  // <Requests for [[GUSIFileToken]]>=                                       
  kWillAccess,
  // [[mkdir]] creates a directory.                                          
  //                                                                         
  // <Requests for [[GUSIFileToken]]>=                                       
  kWillMkdir,
  // [[rmdir]] deletes a directory.                                          
  //                                                                         
  // <Requests for [[GUSIFileToken]]>=                                       
  kWillRmdir,
  // [[opendir]] opens a directory handle on the given directory.            
  //                                                                         
  // <Requests for [[GUSIFileToken]]>=                                       
  kWillOpendir,
  // [[symlink]] creates a symbolic link to a file.                          
  //                                                                         
  // <Requests for [[GUSIFileToken]]>=                                       
  kWillSymlink,
  // [[readlink]] reads the contents of a symbolic link.                     
  //                                                                         
  // <Requests for [[GUSIFileToken]]>=                                       
  kWillReadlink,
  // [[fgetfileinfo]] and [[fsetfileinfo]] reads and set the type and creator
  // code of a file.                                                         
  //                                                                         
  // <Requests for [[GUSIFileToken]]>=                                       
  kWillGetfileinfo,
  kWillSetfileinfo,
  // [[faccess]] manipulates MPW properties of files.                        
  //                                                                         
  // <Requests for [[GUSIFileToken]]>=                                       
  kWillFaccess,
		kNoRequest
	};
	
	GUSIFileToken(const char * path, Request request, bool useAlias = false);
	GUSIFileToken(const GUSIFileSpec & spec, Request request);
	GUSIFileToken(short fRefNum, Request request);
	
	bool	 		IsFile() 		const { return fIsFile; 		}
	bool	 		IsDevice() 		const { return !fIsFile; 		}
	Request			WhichRequest()	const { return fRequest;		}	
	GUSIDevice *	Device()		const {	return fDevice;			}
	const char *	Path()			const { return fPath;			}
	
	static bool		StrFragEqual(const char * name, const char * frag);
	enum StdStream {
		kStdin,
		kStdout,
		kStderr,
		kConsole,
		kNoStdStream = -2
	};				
	static StdStream StrStdStream(const char * name);
	
private:
	GUSIDevice * 	fDevice;
	const char *	fPath;
	bool			fIsFile;
	Request			fRequest;
};
// \section{Definition of [[GUSIDirectory]]}                               
//                                                                         
// [[GUSIDirectory]] is a directory handle to iterate over all entries in a 
// directory.                                                              
//                                                                         
// <Definition of class [[GUSIDirectory]]>=                                
class GUSIDirectory {
public:
	virtual 		 	~GUSIDirectory() {}
	virtual dirent    *	readdir() = 0;
	virtual long 		telldir() = 0;
	virtual void 		seekdir(long pos) = 0;
	virtual void 		rewinddir() = 0;
protected:
	friend class GUSIDevice;
	GUSIDirectory()     {}
};
// \section{Definition of [[GUSIDeviceRegistry]]}                          
//                                                                         
// The [[GUSIDeviceRegistry]] is a singleton class registering all socket  
// domains.                                                                
//                                                                         
// <Definition of class [[GUSIDeviceRegistry]]>=                           
class GUSIDeviceRegistry {
public:
	// The only instance of [[GUSIDeviceRegistry]] is, as usual, obtained by calling
 // [[Instance]].                                                           
 //                                                                         
 // <Creation of [[GUSIDeviceRegistry]]>=                                   
 static  GUSIDeviceRegistry *	Instance();
	// <Operations for [[GUSIDeviceRegistry]]>=                                
 GUSISocket * open(const char * path, int flags);
 // <Operations for [[GUSIDeviceRegistry]]>=                                
 int remove(const char * path);
 // <Operations for [[GUSIDeviceRegistry]]>=                                
 int rename(const char * oldname, const char * newname);
 // <Operations for [[GUSIDeviceRegistry]]>=                                
 int stat(const char * path, struct stat * buf, bool useAlias);
 // <Operations for [[GUSIDeviceRegistry]]>=                                
 int chmod(const char * path, mode_t mode);
 // <Operations for [[GUSIDeviceRegistry]]>=                                
 int utime(const char * path, const utimbuf * times);
 // <Operations for [[GUSIDeviceRegistry]]>=                                
 int access(const char * path, int mode);
 // <Operations for [[GUSIDeviceRegistry]]>=                                
 int mkdir(const char * path);
 // <Operations for [[GUSIDeviceRegistry]]>=                                
 int rmdir(const char * path);
 // <Operations for [[GUSIDeviceRegistry]]>=                                
 GUSIDirectory * opendir(const char * path);
 // <Operations for [[GUSIDeviceRegistry]]>=                                
 int symlink(const char * target, const char * newlink);
 // <Operations for [[GUSIDeviceRegistry]]>=                                
 int readlink(const char * path, char * buf, int bufsize);
 // <Operations for [[GUSIDeviceRegistry]]>=                                
 int fgetfileinfo(const char * path, OSType * creator, OSType * type);
 int fsetfileinfo(const char * path, OSType creator, OSType type);
 // <Operations for [[GUSIDeviceRegistry]]>=                                
 int faccess(const char * path, unsigned * cmd, void * arg);
	// [[AddDevice]] and [[RemoveDevice]] add and remove a [[GUSIDevice]].     
 //                                                                         
 // <Registration interface of [[GUSIDeviceRegistry]]>=                     
 void AddDevice(GUSIDevice * device);
 void RemoveDevice(GUSIDevice * device);
	// It is convenient to define iterators to iterate across all devices.     
 //                                                                         
 // <Iterator on [[GUSIDeviceRegistry]]>=                                   
 class iterator;

 iterator begin();
 iterator end();
protected:
	// On construction, a [[GUSIFileToken]] looks up the appropriate device in the
 // [[GUSIDeviceRegistry]].                                                 
 //                                                                         
 // <Looking up a device in the [[GUSIDeviceRegistry]]>=                    
 friend class GUSIFileToken;

 GUSIDevice *	Lookup(GUSIFileToken & file);
private:
	// <Privatissima of [[GUSIDeviceRegistry]]>=                               
 static GUSIDeviceRegistry *	sInstance;
 // Devices are stored in a linked list. On creation of the registry, it immediately
 // registers the instance for plain Macintosh file sockets, to which pretty much all
 // operations default. This device will never refuse any request.          
 //                                                                         
 // <Privatissima of [[GUSIDeviceRegistry]]>=                               
 GUSIDevice *	fFirstDevice;
 GUSIDeviceRegistry();
};
// \section{Definition of [[GUSIDevice]]}                                  
//                                                                         
// [[GUSIDevice]] consists of a few maintenance functions and the          
// device operations. The request dispatcher first calls [[Want]] for      
// each candidate device and as soon as it's successful, calls the specific
// operation. Devices are kept in a linked list by the [[GUSIDeviceRegistry]].
//                                                                         
// <Definition of class [[GUSIDevice]]>=                                   
class GUSIDevice {
public:
	virtual bool	Want(GUSIFileToken & file);
	
	// <Operations for [[GUSIDevice]]>=                                        
 virtual GUSISocket * open(GUSIFileToken & file, int flags);
 // <Operations for [[GUSIDevice]]>=                                        
 virtual int remove(GUSIFileToken & file);
 // <Operations for [[GUSIDevice]]>=                                        
 virtual int rename(GUSIFileToken & from, const char * newname);
 // <Operations for [[GUSIDevice]]>=                                        
 virtual int stat(GUSIFileToken & file, struct stat * buf);
 // <Operations for [[GUSIDevice]]>=                                        
 virtual int chmod(GUSIFileToken & file, mode_t mode);
 // <Operations for [[GUSIDevice]]>=                                        
 virtual int utime(GUSIFileToken & file, const utimbuf * times);
 // <Operations for [[GUSIDevice]]>=                                        
 virtual int access(GUSIFileToken & file, int mode);
 // <Operations for [[GUSIDevice]]>=                                        
 virtual int mkdir(GUSIFileToken & file);
 // <Operations for [[GUSIDevice]]>=                                        
 virtual int rmdir(GUSIFileToken & file);
 // <Operations for [[GUSIDevice]]>=                                        
 virtual GUSIDirectory * opendir(GUSIFileToken & file);
 // <Operations for [[GUSIDevice]]>=                                        
 virtual int symlink(GUSIFileToken & to, const char * newlink);
 // <Operations for [[GUSIDevice]]>=                                        
 virtual int readlink(GUSIFileToken & link, char * buf, int bufsize);
 // <Operations for [[GUSIDevice]]>=                                        
 virtual int fgetfileinfo(GUSIFileToken & file, OSType * creator, OSType * type);
 virtual int fsetfileinfo(GUSIFileToken & file, OSType creator, OSType type);
 // <Operations for [[GUSIDevice]]>=                                        
 virtual int faccess(GUSIFileToken & file, unsigned * cmd, void * arg);
protected:
	friend class GUSIDeviceRegistry;
	friend class GUSIDeviceRegistry::iterator;
	
	GUSIDevice() : fNextDevice(nil)			{}
	virtual ~GUSIDevice()					{}
	
	GUSIDevice	*	fNextDevice;
};

#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif

// \section{Implementation of [[GUSIDeviceRegistry]]}                      
//                                                                         
//                                                                         
// <Definition of [[GUSISetupDevices]] hook>=                              
extern "C" void GUSISetupDevices();

// <Inline member functions for class [[GUSIDeviceRegistry]]>=             
inline GUSIDeviceRegistry * GUSIDeviceRegistry::Instance()
{
	if (!sInstance) {
		sInstance = new GUSIDeviceRegistry();
		GUSISetupDevices();
	}
	
	return sInstance;
}
// The [[GUSIDeviceRegistry]] forward iterator is simple.                  
//                                                                         
// <Inline member functions for class [[GUSIDeviceRegistry]]>=             
class GUSIDeviceRegistry::iterator {
public:
	iterator(GUSIDevice * device = 0) : fDevice(device) {}
	GUSIDeviceRegistry::iterator & operator++()		
		{ fDevice = fDevice->fNextDevice; return *this; 					}
	GUSIDeviceRegistry::iterator operator++(int)
		{ GUSIDeviceRegistry::iterator old(*this); fDevice = fDevice->fNextDevice; return old; 	}
	bool operator==(const GUSIDeviceRegistry::iterator other) const
								{ return fDevice==other.fDevice; 			}
	GUSIDevice & operator*()	{ return *fDevice;							}
	GUSIDevice * operator->()	{ return fDevice;							}
private:
	GUSIDevice *				fDevice;
};

inline GUSIDeviceRegistry::iterator GUSIDeviceRegistry::begin()		
{ 
	return GUSIDeviceRegistry::iterator(fFirstDevice);	
}

inline GUSIDeviceRegistry::iterator GUSIDeviceRegistry::end()		
{ 
	return GUSIDeviceRegistry::iterator();				
}

#endif /* GUSI_SOURCE */

#endif /* _GUSIDevice_ */
