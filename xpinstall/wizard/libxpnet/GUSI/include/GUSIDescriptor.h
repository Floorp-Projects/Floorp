// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIDescriptor.nw	-	Descriptor Table                          
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIDescriptor.h,v $
// % Revision 1.1  2001/03/11 22:33:48  sgehani%netscape.com
// % First Checked In.
// %                                           
// % Revision 1.15  2001/01/22 04:31:11  neeri                             
// % Last minute changes for 2.1.5                                         
// %                                                                       
// % Revision 1.14  2001/01/17 08:40:17  neeri                             
// % Prevent inlining of overridable functions                             
// %                                                                       
// % Revision 1.13  2000/06/12 04:23:43  neeri                             
// % Return values, not references; Introduce support for multiple descriptor tables
// %                                                                       
// % Revision 1.12  2000/05/23 06:58:03  neeri                             
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.11  2000/03/15 07:14:26  neeri                             
// % Prevent double destruction of descriptor table                        
// %                                                                       
// % Revision 1.10  2000/03/06 06:26:57  neeri                             
// % Introduce (and call) CloseAllDescriptors()                            
// %                                                                       
// % Revision 1.9  1999/08/26 05:45:01  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.8  1999/08/02 07:02:43  neeri                              
// % Support for asynchronous errors and other socket options              
// %                                                                       
// % Revision 1.7  1999/06/30 07:42:05  neeri                              
// % Getting ready to release 2.0b3                                        
// %                                                                       
// % Revision 1.6  1999/05/29 06:26:42  neeri                              
// % Fixed header guards                                                   
// %                                                                       
// % Revision 1.5  1999/04/29 05:00:48  neeri                              
// % Fix bug with bizarre uses of dup2                                     
// %                                                                       
// % Revision 1.4  1999/03/17 09:05:06  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.3  1998/10/11 16:45:12  neeri                              
// % Ready to release 2.0a2                                                
// %                                                                       
// % Revision 1.2  1998/08/01 21:32:03  neeri                              
// % About ready for 2.0a1                                                 
// %                                                                       
// % Revision 1.1  1998/01/25 21:02:44  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// % Revision 1.1  1996/12/16 02:12:40  neeri                              
// % TCP Sockets sort of work                                              
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{Mapping descriptors to sockets}                                
//                                                                         
// POSIX routines do not, of course, operate on [[GUSISockets]] but on     
// numerical descriptors. The [[GUSIDescriptorTable]] singleton maps between
// descriptors and their [[GUSISockets]].                                  
//                                                                         
// <GUSIDescriptor.h>=                                                     
#ifndef _GUSIDescriptor_
#define _GUSIDescriptor_

#ifdef GUSI_SOURCE

#include "GUSISocket.h"

#include <ConditionalMacros.h>

#if PRAGMA_STRUCT_ALIGN
#pragma options align=native
#endif

// \section{Definition of [[GUSIDescriptorTable]]}                         
//                                                                         
// A [[GUSIDescriptorTable]] is another singleton class, behaving in many aspects
// like an array of [[GUSISocket]] pointers. [[InstallSocket]] installs a new socket 
// into the table, picking the first available slot with a descriptor greater than
// or equal to [[start]]. [[RemoveSocket]] empties one slot.               
// [[GUSIDescriptorTable::LookupSocket]] is a shorthand for                
// [[ (*GUSIDescriptorTable::Instance())[fd] ]].                           
//                                                                         
// To allow for light-weight processes, we provide a copy constructor and  
// the [[SetInstance]] member.                                             
//                                                                         
// <Definition of class [[GUSIDescriptorTable]]>=                          
class GUSIDescriptorTable {
public:
	enum { SIZE = 64 };
	
	static GUSIDescriptorTable * 	Instance();
	
	int					InstallSocket(GUSISocket * sock, int start = 0);
	int					RemoveSocket(int fd);
	GUSISocket * 		operator[](int fd);
	static GUSISocket *	LookupSocket(int fd);
	
	class iterator;
	friend class iterator;
	
	iterator			begin();
	iterator			end();
	
	~GUSIDescriptorTable();
	
	static void			CloseAllDescriptors();

	static void SetInstance(GUSIDescriptorTable * table);
	
	GUSIDescriptorTable(const GUSIDescriptorTable & parent);
private:
	// \section{Implementation of [[GUSIDescriptorTable]]}                     
 //                                                                         
 // On creation, a [[GUSIDescriptorTable]] clears all descriptors.          
 //                                                                         
 // <Privatissima of [[GUSIDescriptorTable]]>=                              
 GUSISocket *	fSocket[SIZE];
 int				fInvalidDescriptor;
 GUSIDescriptorTable();
 // <Privatissima of [[GUSIDescriptorTable]]>=                              
 static	GUSIDescriptorTable	* sGUSIDescriptorTable;
};

#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif

// If no instance exists yet, [[GUSIDescriptorTable::Instance]] creates one and
// calls [[GUSISetupConsole]] if the [[setupConsole]] parameter is true.   
// [[GUSISetupConsole]] calls [[GUSIDefaultSetupConsole]], which first calls
// [[GUSISetupConsoleDescriptors]] to set up file descriptors 0, 1, and 2, and
// then calls [[GUSISetupConsoleStdio]] to deal with the necessary initializations
// on the stdio level.                                                     
//                                                                         
// <Hooks for ANSI library interfaces>=                                    
extern "C" {
void GUSISetupConsole();
void GUSIDefaultSetupConsole();
void GUSISetupConsoleDescriptors();
void GUSISetupConsoleStdio();
}
// Destructing a [[GUSIDescriptorTable]] may be a bit problematic, as this 
// may have effects reaching up into the stdio layer. We therefore factor  
// out the stdio aspects into the procedures [[StdioClose]] and [[StdioFlush]]
// which we then can redefine in other, stdio library specific, libraries. 
//                                                                         
// <Hooks for ANSI library interfaces>=                                    
extern "C" {
void GUSIStdioClose();	
void GUSIStdioFlush();
}

// <Inline member functions for class [[GUSIDescriptorTable]]>=            
class GUSIDescriptorTable::iterator {
public:
	iterator(GUSIDescriptorTable * table, int fd = 0) : fTable(table), fFd(fd) {} 	
	GUSIDescriptorTable::iterator & operator++();
	GUSIDescriptorTable::iterator operator++(int);
	int	operator*()				{ return fFd;							}
	bool operator==(const GUSIDescriptorTable::iterator & other) const;
private:
	GUSIDescriptorTable *		fTable;
	int							fFd;
};

inline GUSIDescriptorTable::iterator & GUSIDescriptorTable::iterator::operator++()
{
	while (++fFd < fTable->fInvalidDescriptor && !fTable->fSocket[fFd])
		;
	
	return *this;
}

inline GUSIDescriptorTable::iterator GUSIDescriptorTable::iterator::operator++(int)
{
	int oldFD = fFd;
	
	while (++fFd < fTable->fInvalidDescriptor && !fTable->fSocket[fFd])
		;
	
	return GUSIDescriptorTable::iterator(fTable, oldFD);
}

inline bool GUSIDescriptorTable::iterator::operator==(
				const GUSIDescriptorTable::iterator & other) const
{
	return fFd == other.fFd;
}

inline GUSIDescriptorTable::iterator GUSIDescriptorTable::begin()
{
	return iterator(this);
}

inline GUSIDescriptorTable::iterator GUSIDescriptorTable::end()
{
	return iterator(this, fInvalidDescriptor);
}

#endif /* GUSI_SOURCE */

#endif /* _GUSIDescriptor_ */
