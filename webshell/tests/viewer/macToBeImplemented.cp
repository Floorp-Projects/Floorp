#include "fe_proto.h"
#include "xp_file.h"
#include "prlog.h"

/* Netlib utility routine, should be ripped out */
void	FE_FileType(char * path, Bool * useDefault, char ** fileType, char ** encoding)
{
	return;
}

//
// XP_File routines, stubbed for now

char * WH_FileName (const char *name, XP_FileType type)
{
	PR_ASSERT(FALSE);
}

void XP_CloseDir( XP_Dir dir )
{
	PR_ASSERT(FALSE);
}

XP_File XP_FileOpen( const char* name, XP_FileType type,
					 const XP_FilePerm permissions )
{
	PR_ASSERT(FALSE);
}

char * XP_FileReadLine(char * dest, int32 bufferSize, XP_File file)
{
	PR_ASSERT(FALSE);
}

int XP_FileRemove( const char* name, XP_FileType type )
{
	PR_ASSERT(FALSE);
}
XP_Dir XP_OpenDir( const char* name, XP_FileType type )
{
	PR_ASSERT(FALSE);
}

XP_DirEntryStruct * XP_ReadDir(XP_Dir dir)
{
	PR_ASSERT(FALSE);
}
int XP_Stat( const char* name, XP_StatStruct * outStat,  XP_FileType type )
{
	PR_ASSERT(FALSE);
}

#include "mcom_db.h"
DB * dbopen(const char *fname, int flags,int mode, DBTYPE type, const void *openinfo)
{
	PR_ASSERT(FALSE);
}