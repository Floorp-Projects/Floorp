#ifndef __NS_SoftwareUpdateRun_H__
#define __NS_SoftwareUpdateRun_H__

PRInt32 Install(nsInstallInfo *installInfo);

extern "C" NS_EXPORT PRInt32 Install(const char* jarFile, const char* flags, const char* args, const char* fromURL);

#endif 