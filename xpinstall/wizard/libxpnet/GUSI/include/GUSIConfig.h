// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
// % Project	:	GUSI				-	Grand Unified Socket Interface                    
// % File		:	GUSIConfig.nw		-	Configuration settings                       
// % Author	:	Matthias Neeracher                                           
// % Language	:	C++                                                        
// %                                                                       
// % $Log: GUSIConfig.h,v $
// % Revision 1.1  2001/03/11 22:33:34  sgehani%netscape.com
// % First Checked In.
// %                                               
// % Revision 1.18  2001/01/22 04:31:11  neeri                             
// % Last minute changes for 2.1.5                                         
// %                                                                       
// % Revision 1.17  2001/01/17 08:40:17  neeri                             
// % Prevent inlining of overridable functions                             
// %                                                                       
// % Revision 1.16  2000/05/23 06:54:39  neeri                             
// % Improve formatting, update to latest universal headers                
// %                                                                       
// % Revision 1.15  2000/03/15 07:10:29  neeri                             
// % Fix suffix searching code                                             
// %                                                                       
// % Revision 1.14  2000/03/06 06:24:34  neeri                             
// % Fix plausibility tests for A5                                         
// %                                                                       
// % Revision 1.13  1999/09/26 03:56:44  neeri                             
// % Sanity check for A5                                                   
// %                                                                       
// % Revision 1.12  1999/08/26 05:44:59  neeri                             
// % Fixes for literate edition of source code                             
// %                                                                       
// % Revision 1.11  1999/06/28 05:57:03  neeri                             
// % Support SIGINT generation                                             
// %                                                                       
// % Revision 1.10  1999/05/29 06:26:41  neeri                             
// % Fixed header guards                                                   
// %                                                                       
// % Revision 1.9  1999/03/29 09:51:28  neeri                              
// % New configuration system with support for hardcoded configurations.   
// %                                                                       
// % Revision 1.8  1999/03/17 09:05:05  neeri                              
// % Added GUSITimer, expanded docs                                        
// %                                                                       
// % Revision 1.7  1998/10/11 16:45:10  neeri                              
// % Ready to release 2.0a2                                                
// %                                                                       
// % Revision 1.6  1998/08/01 21:32:01  neeri                              
// % About ready for 2.0a1                                                 
// %                                                                       
// % Revision 1.5  1998/01/25 20:53:52  neeri                              
// % Engine implemented, except for signals & scheduling                   
// %                                                                       
// % Revision 1.4  1997/11/13 21:12:10  neeri                              
// % Fall 1997                                                             
// %                                                                       
// % Revision 1.3  1996/11/24  13:00:27  neeri                             
// % Fix comment leaders                                                   
// %                                                                       
// % Revision 1.2  1996/11/24  12:52:06  neeri                             
// % Added GUSIPipeSockets                                                 
// %                                                                       
// % Revision 1.1.1.1  1996/11/03  02:43:32  neeri                         
// % Imported into CVS                                                     
// %                                                                       
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  
//                                                                         
// \chapter{GUSI Configuration settings}                                   
//                                                                         
// GUSI stores its global configuration settings in the [[GUSIConfiguration]] 
// singleton class. To create the instance, GUSI calls the [[GUSISetupConfig]]
// hook.                                                                   
//                                                                         
// <GUSIConfig.h>=                                                         
#ifndef _GUSIConfig_
#define _GUSIConfig_

#ifdef GUSI_SOURCE

#include "GUSIFileSpec.h"

#include <ConditionalMacros.h>

#if PRAGMA_STRUCT_ALIGN
#pragma options align=native
#endif

// \section{Definition of configuration settings}                          
//                                                                         
// The GUSIConfiguration has a single instance with read only access, accessible
// with the static [[Instance]] member function.                           
//                                                                         
// <Definition of class [[GUSIConfiguration]]>=                            
class GUSIConfiguration {
public:
	enum { kNoResource = -1, kDefaultResourceID = 10240 };
	
	static GUSIConfiguration * 	Instance();
	static GUSIConfiguration *	CreateInstance(short resourceID = kDefaultResourceID);
	
	// To determine the file type and creator of a newly created file, we first try 
 // to match one of the [[FileSuffix]] suffices.                            
 //                                                                         
 // <Type and creator rules for newly created files>=                       
 struct FileSuffix {
 	char 	suffix[4];
 	OSType	suffType;
 	OSType	suffCreator;
 };
 short			fNumSuffices;
 FileSuffix *	fSuffices;

 void ConfigureSuffices(short numSuffices, FileSuffix * suffices);
 // If none of the suffices matches, we apply the default type and creator. These
 // rules are applied with [[SetDefaultFType]].                             
 //                                                                         
 // <Type and creator rules for newly created files>=                       
 OSType		fDefaultType;
 OSType		fDefaultCreator;

 void ConfigureDefaultTypeCreator(OSType defaultType, OSType defaultCreator);
 void SetDefaultFType(const GUSIFileSpec & name) const;
	// To simplify Macintosh friendly ports of simple, I/O bound programs it is 
 // possible to specify automatic yielding on read() and write() calls.     
 // [[AutoSpin]] will spin a cursor and/or yield the CPU if desired.        
 //                                                                         
 // <Automatic cursor spin>=                                                
 bool fAutoSpin;

 void ConfigureAutoSpin(bool autoSpin);
 void AutoSpin() const;
	// GUSI applications can crash hard if QuickDraw is not initialized. Therefore, we
 // offer to initialize it automatically with the [[fAutoInitGraf]] feature.
 //                                                                         
 // <Automatic initialization of QuickDraw>=                                
 bool fAutoInitGraf;

 void ConfigureAutoInitGraf(bool autoInitGraf);
 void AutoInitGraf();
	// Due to the organization of a UNIX filesystem, it is fairly easy to find 
 // out how many subdirectories a given directory has, since the [[nlink]] field of 
 // its inode will automatically contain the number of subdirectories[[+2]]. Therefore,
 // some UNIX derived code depends on this behaviour. When [[fAccurateStat]] is set, 
 // GUSI emulates this behaviour, but be warned that this makes [[stat]] on 
 // directories a much more expensive operation. If [[fAccurateStat]] is not set,
 // stat() gives the total number of entries in the directory[[+2]] as a conservative 
 // estimate.                                                               
 //                                                                         
 // <Various flags>=                                                        
 bool fAccurateStat;

 void ConfigureAccurateStat(bool accurateState);
 // The [[fSigPipe]] feature causes a signal [[SIGPIPE]] to be raised if an attempt
 // is made to write to a broken pipe.                                      
 //                                                                         
 // <Various flags>=                                                        
 bool		fSigPipe;

 void ConfigureSigPipe(bool sigPipe);
 void BrokenPipe();
 // The [[fSigInt]] feature causes a signal [[SIGINT]] to be raised if the user presses
 // command-period.                                                         
 //                                                                         
 // <Various flags>=                                                        
 bool		fSigInt;

 void ConfigureSigInt(bool sigInt);
 void CheckInterrupt();
 // If [[fSharedOpen]] is set, open() opens files with shared read/write permission.
 //                                                                         
 // <Various flags>=                                                        
 bool		fSharedOpen;

 void ConfigureSharedOpen(bool sharedOpen);
 // If [[fHandleAppleEvents]] is set, GUSI automatically handles AppleEvents in its 
 // event handling routine.                                                 
 //                                                                         
 // <Various flags>=                                                        
 bool		fHandleAppleEvents;

 void ConfigureHandleAppleEvents(bool handleAppleEvents);
protected:
	GUSIConfiguration(short resourceID = kDefaultResourceID);
private:
	// \section{Implementation of configuration settings}                      
 //                                                                         
 // The sole instance of [[GUSIConfiguration]] is created on demand.        
 //                                                                         
 // <Privatissima of [[GUSIConfiguration]]>=                                
 static GUSIConfiguration * sInstance;
 // [[ConfigureSuffices]] sets up the suffix table.                         
 //                                                                         
 // <Privatissima of [[GUSIConfiguration]]>=                                
 bool fWeOwnSuffices;
 // [[AutoSpin]] tests the flag inline, but performs the actual spinning out of
 // line.                                                                   
 //                                                                         
 // <Privatissima of [[GUSIConfiguration]]>=                                
 void DoAutoSpin() const;
 // [[AutoInitGraf]] works rather similarly to [[AutoSpin]].                
 //                                                                         
 // <Privatissima of [[GUSIConfiguration]]>=                                
 void DoAutoInitGraf();
 // [[CheckInterrupt]] raises a [[SIGINT]] signal if desired.               
 //                                                                         
 // <Privatissima of [[GUSIConfiguration]]>=                                
 bool CmdPeriod(const EventRecord * event);
};

#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif

// To create the sole instance of [[GUSIConfiguration]], we call [[GUSISetupConfig]]
// which has to call [[GUSIConfiguration::CreateInstance]].                
//                                                                         
// <Definition of [[GUSISetupConfig]] hook>=                               
#ifdef __MRC__
#pragma noinline_func GUSISetupConfig
#endif

extern "C" void GUSISetupConfig();
// <Inline member functions for class [[GUSIConfiguration]]>=              
inline GUSIConfiguration * GUSIConfiguration::Instance()
{
	if (!sInstance)
		GUSISetupConfig();
	if (!sInstance)
		sInstance = new GUSIConfiguration();
	
	return sInstance;
}

inline GUSIConfiguration * GUSIConfiguration::CreateInstance(short resourceID)
{
	if (!sInstance)
		sInstance = new GUSIConfiguration(resourceID);
	
	return sInstance;
}
// <Inline member functions for class [[GUSIConfiguration]]>=              
inline void GUSIConfiguration::ConfigureDefaultTypeCreator(OSType defaultType, OSType defaultCreator)
{
	fDefaultType	= defaultType;
	fDefaultCreator	= defaultCreator;
}
// <Inline member functions for class [[GUSIConfiguration]]>=              
inline void GUSIConfiguration::ConfigureAutoSpin(bool autoSpin)
{
	fAutoSpin = autoSpin;
}
// <Inline member functions for class [[GUSIConfiguration]]>=              
inline void GUSIConfiguration::AutoSpin() const
{
	if (fAutoSpin)
		DoAutoSpin();
}
// <Inline member functions for class [[GUSIConfiguration]]>=              
inline void GUSIConfiguration::ConfigureAutoInitGraf(bool autoInitGraf)
{
	fAutoInitGraf 	= autoInitGraf;
}
// <Inline member functions for class [[GUSIConfiguration]]>=              
inline void GUSIConfiguration::AutoInitGraf()
{
	if (fAutoInitGraf)
		DoAutoInitGraf();
}
// <Inline member functions for class [[GUSIConfiguration]]>=              
inline void GUSIConfiguration::ConfigureSigPipe(bool sigPipe)
{
	fSigPipe 	= sigPipe;
}
// <Inline member functions for class [[GUSIConfiguration]]>=              
inline void GUSIConfiguration::ConfigureSigInt(bool sigInt)
{
	fSigInt 	= sigInt;
}
// <Inline member functions for class [[GUSIConfiguration]]>=              
inline void GUSIConfiguration::ConfigureAccurateStat(bool accurateStat)
{
	fAccurateStat 	= accurateStat;
}
inline void GUSIConfiguration::ConfigureSharedOpen(bool sharedOpen)
{
	fSharedOpen 	= sharedOpen;
}
#endif /* GUSI_SOURCE */

#endif /* _GUSIConfig_ */
