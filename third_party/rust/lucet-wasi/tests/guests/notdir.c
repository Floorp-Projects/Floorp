#include <assert.h>
#include <errno.h>
#include <dirent.h>

int main()
{
    DIR *dir = opendir("/sandbox/notadir");
    assert(dir == NULL);
    assert(errno == ENOTDIR);

    return 0;
}
