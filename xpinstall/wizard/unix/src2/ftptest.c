#include <stdio.h>
#include <stdlib.h>

#ifndef NULL
#define NULL 0
#endif

int 
main(int arc, char **argv)
{
    char *ftphost = "sweetlou";
    char *file = "/products/server/README";
    char *basename = "./README";
    char *user = "test";
    char *localhost = "test.com";
    char *ftpcmds = malloc(1024);

    sprintf(ftpcmds, "\
\
ftp -n %s <<EndFTP\n\
binary\n\
user anonymous %s@%s\n\
get %s %s\n\
EndFTP\n\
\
", ftphost, user, localhost, file, basename);

    printf(ftpcmds);
    system(ftpcmds);

    return 0;
}
