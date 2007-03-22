// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIFileSpec.nw		-	File specifications                        
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIFileSpec.h,v $
// % Revision 1.2  2001/12/23 23:23:41  timeless%mac.com
// % Bugzilla Bug 106386 rid source of these misspellings: persistant persistance priviledge protocal editting editted targetted targetting
// % r='s from many people. sr=jst
// %
// % Revision 1.1  2001/03/11 22:35:07  sgehani%netscape.com
// % First Checked In.
// %                                             
// % Revision 1.15  2001/01/17 08:46:45  neeri                             
// % Get rid of excess directory separators when name is empty             
// %                                                                       
// % Revision 1.14  2000/12/23 06:10:17  neeri                             
// % Add GUSIFSpTouchFolder, GUSIFSpGetCatInfo; copy error in copy constructor
// %                                                                       
// % Revision 1.13  2000/10/16 03:59:36  neeri                             
// % Make FSSpec a member of GUSIFileSpec instead of a base class          
// %                                                                       
// % Revision 1.12  2000/06/12 04:20:58  neeri                             
// % Introduce GUSI_*printf                                                
// %                                                                       
// % Revision 1.11  2000/05/23 07:00:00  neeri                             
// % Improve formatting                                                    
// %                                                                       
// % Revision 1.10  2000/03/15 07:16:08  neeri                             
// % Fix path construction, temp name bugs                                 
// %                                                                       
// % Revision 1.9  2000/03/06 06:34:11  neeri                              
// % Added C FSSpec convenience calls                                      
// %                                                                       
// % Revision 1.8  1999/08/26 05:45:02  neeri                              
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.7  1999/07/19 06:21:02  neeri                              
// % Add mkdir/rmdir, fix various file manager related bugs                
// %                                                                       
// % Revision 1.6  1999/06/28 06:00:53  neeri                              
// % add support for generating temp names from basename                   
// %                                                                       
// % Revision 1.5  1999/05/30 02:16:53  neeri                              
// % Cleaner definition of GUSICatInfo                                     
// %                                                                       
// % Revision 1.4  1999/03/17 09:05:07  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.3  1998/10/11 16:45:15  neeri                              
// % Ready to release 2.0a2                                                
// %                                                                       
// % Revision 1.2  1998/08/01 21:32:04  neeri                              
// % About ready for 2.0a1                                                 
// %                                                                       
// % Revision 1.1  1998/01/25 21:02:46  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{File specifications}                                           
//                                                                         
// While the Macintosh toolbox has a convenient type [[FSSpec]] to pass to 
// file system routines, it lacks manipulation functions. We provide them here.
// This module has been known under a different name and with different data types
// in GUSI 1.                                                              
//                                                                         
// <GUSIFileSpec.h>=                                                       
#ifndef _GUSIFileSpec_
#define _GUSIFileSpec_

#include <MacTypes.h>
#include <Files.h>
#include <Folders.h>

#include <string.h>
#include <sys/cdefs.h>

__BEGIN_DECLS
// \section{C Interfaces to [[GUSIFileSpec]]}                              
//                                                                         
// Many of the API routines defined here are useful to C code and not too hard to make accessible,
// even though I prefer the C++ versions. As opposed to GUSI 1, we stick to our namespace now.
//                                                                         
// <Plain C interfaces to [[GUSIFileSpec]]>=                               
/*
 * Construct a FSSpec from...
 */
/* ... the refNum of an open file. 		*/
OSErr GUSIFRefNum2FSp(short fRefNum, FSSpec * desc);
/* ... a working directory & file name. */
OSErr GUSIWD2FSp(short wd, ConstStr31Param name, FSSpec * desc);
/* ... a path name.						*/
OSErr GUSIPath2FSp(const char * path, FSSpec * desc);	
/* ... a special object. 				*/
OSErr GUSISpecial2FSp(OSType object, short vol, FSSpec * desc);	
/* ... a temporary file path.		    */
OSErr GUSIMakeTempFSp(short vol, long dirID, FSSpec * desc);

/*
 * Convert a FSSpec into...
 */
/* ... a full path name.								*/
char * GUSIFSp2FullPath(const FSSpec * desc);
/* ... a relative path name if possible, full if not 	*/
char * GUSIFSp2RelPath(const FSSpec * desc);	
/* ... a path relative to the specified directory 		*/
char * GUSIFSp2DirRelPath(const FSSpec * desc, const FSSpec * dir);
/* ... an GUSI-=specific ASCII encoding. 				*/
char * GUSIFSp2Encoding(const FSSpec * desc);

/*
 * Construct FSSpec of...
 */
/* ... (vRefNum, parID) */
OSErr GUSIFSpUp(FSSpec * desc);
/* ... file (name) in directory denoted by desc */
OSErr GUSIFSpDown(FSSpec * desc, ConstStr31Param name);
/* ... of nth file in directory denoted by (vRefNum, parID) */
OSErr GUSIFSpIndex(FSSpec * desc, short n);

/* Resolve if alias file */
OSErr GUSIFSpResolve(FSSpec * spec);

/* Touch folder containing the object */
OSErr GUSIFSpTouchFolder(const FSSpec * spec);

/* Get catalog information */
OSErr GUSIFSpGetCatInfo(const FSSpec * spec, CInfoPBRec * info);
__END_DECLS

#ifdef GUSI_SOURCE

#include "GUSIBasics.h"
#include "GUSIContext.h"

#include <ConditionalMacros.h>

#if PRAGMA_STRUCT_ALIGN
#pragma options align=native
#endif

// \section{Definition of [[GUSICatInfo]]}                                 
//                                                                         
// [[GUSICatInfo]] is a wrapper for [[CInfoPBRec]]. Since the latter is a [[union]], we cannot
// inherit from it.                                                        
//                                                                         
// <Definition of class [[GUSICatInfo]]>=                                  
class GUSICatInfo {
	CInfoPBRec	fInfo;
public:
	bool		IsFile() const;
	bool		IsAlias() const;
	bool 		DirIsExported() const;
	bool	 	DirIsMounted() const;
	bool	 	DirIsShared() const;
	bool	 	HasRdPerm() const;
	bool 		HasWrPerm() const;
	bool 		Locked() const;
	
	CInfoPBRec &				Info()			{	return fInfo; 							}
	operator CInfoPBRec &()						{	return fInfo; 							}
	struct HFileInfo &			FileInfo()		{	return fInfo.hFileInfo;					}
	struct DirInfo &			DirInfo()		{	return fInfo.dirInfo;					}
	struct FInfo &				FInfo()			{	return fInfo.hFileInfo.ioFlFndrInfo;	}
	struct FXInfo &				FXInfo()		{	return fInfo.hFileInfo.ioFlXFndrInfo;	}
	
	const CInfoPBRec &			Info() const	{	return fInfo; 							}
	operator const CInfoPBRec &()	const		{	return fInfo; 							}
	const struct HFileInfo &	FileInfo() const{	return fInfo.hFileInfo;					}
	const struct DirInfo &		DirInfo() const	{	return fInfo.dirInfo;					}
	const struct FInfo &		FInfo() const	{	return fInfo.hFileInfo.ioFlFndrInfo;	}
	const struct FXInfo &		FXInfo() const	{	return fInfo.hFileInfo.ioFlXFndrInfo;	}
};
// \section{Definition of [[GUSIFileSpec]]}                                
//                                                                         
// A [[GUSIFileSpec]] is a manipulation friendly version of an [[FSSpec]]. Due to
// conversion operators, a [[GUSIFileSpec]] can be used whereever a [[const FSSpec]]
// is specified. For any long term storage, [[FSSpec]] should be used rather than the
// much bulkier [[GUSIFileSpec]].                                          
//                                                                         
// <Definition of class [[GUSIFileSpec]]>=                                 
class GUSIFileSpec {
public:
	// A [[GUSIFileSpec]] can be constructed in lots of different ways. Most of the 
 // constructors have a final argument [[useAlias]] that, when [[true]], suppresses
 // resolution of leaf aliases.                                             
 //                                                                         
 // First, two trivial cases: The default constructor and the copy constructor, which
 // do exactly what you'd expect them to do.                                
 //                                                                         
 // <Constructors for [[GUSIFileSpec]]>=                                    
 GUSIFileSpec()	{}
 GUSIFileSpec(const GUSIFileSpec & spec);
 // The [[FSSpec]] conversion is almost a copy constructor, but by default resolves
 // leaf aliases.                                                           
 //                                                                         
 // <Constructors for [[GUSIFileSpec]]>=                                    
 GUSIFileSpec(const FSSpec & spec, bool useAlias = false);
 // A number of convenience constructors let you construct a [[GUSIFileSpec]] from
 // various components.                                                     
 //                                                                         
 // <Constructors for [[GUSIFileSpec]]>=                                    
 // Construct from volume reference number, directory ID & file name
 GUSIFileSpec(short vRefNum, long parID, ConstStr31Param name, bool useAlias = false);

 // Construct from working directory & file name
 GUSIFileSpec(short wd, ConstStr31Param name, bool useAlias = false);

 // Construct from full or relative path
 GUSIFileSpec(const char * path, bool useAlias = false);
 	
 // Construct from open file reference number
 explicit GUSIFileSpec(short fRefNum);
 // Finally, there is a constructor for constructing a [[GUSIFileSpec]] for one of
 // the folders obtainable with [[FindFolder]].                             
 //                                                                         
 // <Constructors for [[GUSIFileSpec]]>=                                    
 GUSIFileSpec(OSType object, short vol = kOnSystemDisk);
	// All [[GUSIFileSpecs]] have an error variable from which the last error  
 // is available.                                                           
 //                                                                         
 // <Error handling in [[GUSIFileSpec]]>=                                   
 OSErr		Error() const;
 			operator void*() const;
 bool		operator!() const;
	// While earlier versions of GUSI maintained a notion of "current directory" that
 // was independent of the HFS default directory, there is no such distinction anymore
 // in GUSI 2. [[SetDefaultDirectory]] sets the default directory to [[vRefNum, dirID]].
 // [[GetDefaultDirectory]] sets [[vRefNum,dirID]] to the default directory. Neither
 // routine affects the [[name]]. [[GetVolume]] gets the named or indexed volume.
 //                                                                         
 // <Default directory handling in [[GUSIFileSpec]]>=                       
 OSErr	SetDefaultDirectory();
 OSErr	GetDefaultDirectory();
 OSErr	GetVolume(short index = -1);
	// Conversions of [[GUSIFileSpec]] to [[FSSpec]] values and references is, of course, 
 // simple. Pointers are a bit trickier; we define an custom [[operator&]] and two 
 // smart pointer classes. [[operator->]], however, is simpler.             
 //                                                                         
 // <Converting a [[GUSIFileSpec]] to a [[FSSpec]]>=                        
 operator const FSSpec &() const;

 class pointer {
 public:
 	pointer(GUSIFileSpec * ptr);
 	operator GUSIFileSpec *() const;
 	operator const FSSpec *() const;
 private:
 	GUSIFileSpec * ptr;
 };
 pointer operator&();

 class const_pointer {
 public:
 	const_pointer(const GUSIFileSpec * ptr);
 	operator const GUSIFileSpec *() const;
 	operator const FSSpec *() const;
 private:
 	const GUSIFileSpec * ptr;
 };
 const_pointer operator&() const;

 const FSSpec * operator->() const;

 friend class pointer;
 friend class const_pointer;
	// Each [[GUSIFileSpec]] has an implicit [[GUSICatInfo]] record associated with it.
 // [[CatInfo]] calls [[GetCatInfo]] if the record is not already valid and 
 // returns a pointer to the record. [[DirInfo]] replaces the [[GUSIFileSpec]] by 
 // the specification of its parent directory and returns a pointer to information 
 // about the parent. Both calls return a [[nil]] pointer if the object does not exist.
 // If all you are interested in is whether the object exists, call [[Exists]]. 
 //                                                                         
 // <Getting the [[GUSICatInfo]] for a [[GUSIFileSpec]]>=                   
 const GUSICatInfo * CatInfo(short index);
 const GUSICatInfo * DirInfo();
 const GUSICatInfo * CatInfo() const;
 bool				Exists() const;
	// In many POSIXish contexts, it's necessary to specify path names with C string. 
 // Although this practice is dangerous on a Mac, we of course have to conform to
 // it. The basic operations are getting a full path and getting a path relative to
 // a directory (the current directory by default).                         
 //                                                                         
 // <Getting path names from a [[GUSIFileSpec]]>=                           
 char *	FullPath() const;
 char *	RelativePath() const;
 char *	RelativePath(const FSSpec & dir) const;
 // If the path ultimately is going to flow back into a GUSI routine again, it is 
 // possible to simply encode the [[GUSIFileSpec]] in ASCII. This is fast and reliable,
 // but you should of course not employ it with any non-GUSI destination routine and 
 // should never store such a part across a reboot. The standard [[GUSIFileSpec]] 
 // constructors for paths will accept encoded paths.                       
 //                                                                         
 // The encoding is defined as:                                             
 //                                                                         
 // \begin{itemize}                                                         
 // \item 1 byte:   DC1  (ASCII 0x11)                                       
 // \item 4 bytes:  Volume reference number in zero-padded hex              
 // \item 8 bytes:  Directory ID in zero-padded hex                         
 // \item n bytes:  Partial pathname, starting with ':'                     
 // \end{itemize}                                                           
 //                                                                         
 // <Getting path names from a [[GUSIFileSpec]]>=                           
 char *	EncodedPath() const;
	// Most aliases are resolved implicitly, but occasionally you may want to do
 // explicit resolution. [[Resolve]] resolves an alias. If [[gently]] is set, 
 // nonexistent target files of aliases don't cause an error to be returned.
 //                                                                         
 // <Alias resolution for a [[GUSIFileSpec]]>=                              
 OSErr	Resolve(bool gently = true);
 // [[AliasPath]] returns the path an alias points to without mounting any volumes. 
 //                                                                         
 // <Alias resolution for a [[GUSIFileSpec]]>=                              
 char *	AliasPath() const;
	// A major feature of the [[GUSIFileSpec]] class is the set of operators for
 // manipulating a file specification.                                      
 //                                                                         
 // [[operator--]] replaces a file specification with the directory specification of
 // its parent.                                                             
 //                                                                         
 // <Manipulating a [[GUSIFileSpec]]>=                                      
 GUSIFileSpec &	operator--();
 // [[operator++]] sets the [[dirID]] of a directory specification to the ID of the
 // directory.                                                              
 //                                                                         
 // <Manipulating a [[GUSIFileSpec]]>=                                      
 GUSIFileSpec &	operator++();
 // The two versions of [[operator+=]], which internally both call [[AddPathComponent]],
 // replace a directory specification with the specification                
 //                                                                         
 // <Manipulating a [[GUSIFileSpec]]>=                                      
 GUSIFileSpec &	AddPathComponent(const char * name, int length, bool fullSpec);
 GUSIFileSpec &	operator+=(ConstStr31Param name);
 GUSIFileSpec &	operator+=(const char * name);
 // [[operator+]] provides a non-destructive variant of [[operator+=]].     
 //                                                                         
 // <Manipulating a [[GUSIFileSpec]]>=                                      
 friend GUSIFileSpec operator+(const FSSpec & spec, ConstStr31Param name);
 friend GUSIFileSpec operator+(const FSSpec & spec, const char * name);
 // Array access replaces the file specification with the [[index]]th object in the
 // {\em parent directory} of the specification (allowing the same specification to
 // be reused for reading a directory).                                     
 //                                                                         
 // <Manipulating a [[GUSIFileSpec]]>=                                      
 GUSIFileSpec & operator[](short index);
 // To manipulate the fields of the file specification directly, there is a procedural
 // interface.                                                              
 //                                                                         
 // <Manipulating a [[GUSIFileSpec]]>=                                      
 void SetVRef(short vref);
 void SetParID(long parid);
 void SetName(ConstStr63Param nam);
 void SetName(const char * nam);
 // For some modifications to propagate quickly, the surrounding folder needs to 
 // have its date modified.                                                 
 //                                                                         
 // <Manipulating a [[GUSIFileSpec]]>=                                      
 OSErr TouchFolder();
	// Two [[GUSIFileSpecs]] can be compared for (in)equality.                 
 //                                                                         
 // <Comparing two [[GUSIFileSpec]] objects>=                               
 friend bool operator==(const GUSIFileSpec & one, const GUSIFileSpec & other);
 // [[IsParentOf]] determines whether the object specifies a parent directory of 
 // [[other]].                                                              
 //                                                                         
 // <Comparing two [[GUSIFileSpec]] objects>=                               
 bool	IsParentOf(const GUSIFileSpec & other) const;
protected:
	// \section{Implementation of [[GUSIFileSpec]]}                            
 //                                                                         
 // [[sError]] contains the error code for failed calls. [[Error]] returns the value.
 //                                                                         
 // <Privatissima of [[GUSIFileSpec]]>=                                     
 mutable OSErr	fError;
 // For path name constructions, a sometimes large scratch area is needed which is 
 // maintained in [[sScratch]]. A scratch request preserves a preexisting scratch area
 // at the {\em end} of the new area if [[extend]] is nonzero.              
 //                                                                         
 // <Privatissima of [[GUSIFileSpec]]>=                                     
 static char *		sScratch;
 static long			sScratchSize;

 static char *		CScratch(bool extend = false);
 static StringPtr	PScratch();
 // A [[GUSIFileSpec]] has a [[GUSICatInfo]] embedded and a flag [[fValidInfo]] indicating
 // that it is valid.                                                       
 //                                                                         
 // <Privatissima of [[GUSIFileSpec]]>=                                     
 FSSpec							fSpec;
 GUSIIOPBWrapper<GUSICatInfo>	fInfo;
 bool							fValidInfo;
 // For convenience, we define a fictivous parent directory of all volumes. 
 //                                                                         
 // <Privatissima of [[GUSIFileSpec]]>=                                     
 enum { ROOT_MAGIC_COOKIE = 666 };
 // Each accumulation step is preformed in [[PrependPathComponent]].        
 //                                                                         
 // <Privatissima of [[GUSIFileSpec]]>=                                     
 OSErr PrependPathComponent(char *&path, ConstStr63Param component, bool colon) const;
};
// A [[GUSITempFileSpec]] is just a [[GUSIFileSpec]] with constructors to construct
// filenames for temporary files.                                          
//                                                                         
// <Definition of class [[GUSIFileSpec]]>=                                 
class GUSITempFileSpec : public GUSIFileSpec {
public:
	GUSITempFileSpec(short vRefNum = kOnSystemDisk);
	GUSITempFileSpec(short vRefNum, long parID);
	GUSITempFileSpec(ConstStr31Param basename);
	GUSITempFileSpec(short vRefNum, ConstStr31Param basename);
	GUSITempFileSpec(short vRefNum, long parID, ConstStr31Param basename);
private:
	void TempName();
	void TempName(ConstStr31Param basename);
	
	static int	sID;
};

#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif

// \section{Implementation of [[GUSICatInfo]]}                             
//                                                                         
// All of the member functions for [[GUSICatInfo]] are inline.             
//                                                                         
// <Inline member functions for class [[GUSICatInfo]]>=                    
inline bool GUSICatInfo::IsFile() const
{
	return !(DirInfo().ioFlAttrib & 0x10);
}

inline bool GUSICatInfo::IsAlias() const
{
	return
		!(FileInfo().ioFlAttrib & 0x10) &&
		(FInfo().fdFlags & (1 << 15));
}

inline bool GUSICatInfo::DirIsExported() const
{
	return (FileInfo().ioFlAttrib & 0x20) != 0;
}

inline bool GUSICatInfo::DirIsMounted() const
{
	return (FileInfo().ioFlAttrib & 0x08) != 0;
}

inline bool GUSICatInfo::DirIsShared() const
{
	return (FileInfo().ioFlAttrib & 0x04) != 0;
}

inline bool GUSICatInfo::HasRdPerm() const
{
	return !(DirInfo().ioACUser & 0x02) != 0;
}

inline bool GUSICatInfo::HasWrPerm() const
{
	return !(DirInfo().ioACUser & 0x04) != 0;
}

inline bool GUSICatInfo::Locked() const
{
	return (FileInfo().ioFlAttrib & 0x11) == 0x01;
}
// <Inline member functions for class [[GUSIFileSpec]]>=                   
inline OSErr GUSIFileSpec::Error() const
{
	return fError;
}

inline GUSIFileSpec::operator void*() const
{
	return (void *)!fError;
}

inline bool GUSIFileSpec::operator!() const
{
	return fError!=0;
}
// <Inline member functions for class [[GUSIFileSpec]]>=                   
inline StringPtr GUSIFileSpec::PScratch()
{
	return (StringPtr) CScratch();
}
// <Inline member functions for class [[GUSIFileSpec]]>=                   
inline OSErr GUSIFileSpec::SetDefaultDirectory()
{
	return fError = HSetVol(nil, fSpec.vRefNum, fSpec.parID);
}

inline OSErr GUSIFileSpec::GetDefaultDirectory()
{
	fSpec.name[0] 	= 0;
	fValidInfo		= false;
	return fError 	= HGetVol(nil, &fSpec.vRefNum, &fSpec.parID);
}
// [[operator+=]] and [[operator+]] are merely wrappers around [[AddPathComponent]].
//                                                                         
// <Inline member functions for class [[GUSIFileSpec]]>=                   
inline GUSIFileSpec &	GUSIFileSpec::operator+=(ConstStr31Param name)
{
	return AddPathComponent((char *) name+1, name[0], true);
}

inline GUSIFileSpec &	GUSIFileSpec::operator+=(const char * name)
{
	return AddPathComponent(name, strlen(name), true);
}
// The other variations of the call are simple.                            
//                                                                         
// <Inline member functions for class [[GUSIFileSpec]]>=                   
inline const GUSICatInfo * GUSIFileSpec::CatInfo() const
{
	return const_cast<GUSIFileSpec *>(this)->CatInfo(0);
}

inline const GUSICatInfo * GUSIFileSpec::DirInfo()
{
	if (CatInfo(-1)) {
		fSpec.parID = fInfo->DirInfo().ioDrParID;
		fValidInfo	= true;
		
		return &fInfo.fPB;
	} else
		return nil;
}

inline bool GUSIFileSpec::Exists() const
{
	return CatInfo() != nil;
}
// Reference conversion is straightforward, as is [[operator->]].          
//                                                                         
// <Inline member functions for class [[GUSIFileSpec]]>=                   
inline GUSIFileSpec::operator const FSSpec &() const
{
	return fSpec;
}
inline const FSSpec * GUSIFileSpec::operator->() const
{
	return &fSpec;
}
// Pointers, however, are a trickier issue, as they might be used either as a 
// [[GUSIFileSpec *]] or as an [[FSSpec *]].                               
//                                                                         
// <Inline member functions for class [[GUSIFileSpec]]>=                   
inline GUSIFileSpec::const_pointer::const_pointer(const GUSIFileSpec * ptr)
	: ptr(ptr)
{
}
inline GUSIFileSpec::const_pointer::operator const GUSIFileSpec *() const
{
	return ptr;
}
inline GUSIFileSpec::const_pointer::operator const FSSpec *() const
{
	return &ptr->fSpec;
}
inline GUSIFileSpec::const_pointer GUSIFileSpec::operator&() const
{
	return const_pointer(this);
}
// [[GUSIFileSpec::pointer]] is the non-constant equivalent to [[GUSIFileSpec::const_pointer]].
//                                                                         
// <Inline member functions for class [[GUSIFileSpec]]>=                   
inline GUSIFileSpec::pointer::pointer(GUSIFileSpec * ptr)
	: ptr(ptr)
{
}
inline GUSIFileSpec::pointer::operator GUSIFileSpec *() const
{
	return ptr;
}
inline GUSIFileSpec::pointer::operator const FSSpec *() const
{
	return &ptr->fSpec;
}
inline GUSIFileSpec::pointer GUSIFileSpec::operator&()
{
	return pointer(this);
}

#endif /* GUSI_SOURCE */

#endif /* GUSIFileSpec */
