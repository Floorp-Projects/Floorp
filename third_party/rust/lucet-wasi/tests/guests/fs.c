#include <sys/stat.h>
#include <sys/time.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

int main(void)
{
    struct timespec times[2];
    struct dirent * entry;
    char            buf[4];
    DIR *           dir;
    FILE *          fp;
    struct stat     st;
    off_t           offset;
    int             fd;
    int             res;

    fd = open("/sandbox/testfile", O_CREAT | O_RDWR, 0644);
    assert(fd != -1);

    res = posix_fallocate(fd, 0, 10000);
    assert(res == 0);

    res = fstat(fd, &st);
    assert(res == 0);
    assert(st.st_size == 10000);

    res = ftruncate(fd, 1000);
    res = fstat(fd, &st);
    assert(res == 0);
    assert(st.st_size == 1000);
    assert(st.st_nlink == 1);
    res = posix_fadvise(fd, 0, 1000, POSIX_FADV_RANDOM);
    assert(res == 0);

    res = (int) write(fd, "test", 4);
    assert(res == 4);

    offset = lseek(fd, 0, SEEK_CUR);
    assert(offset == 4);
    offset = lseek(fd, 0, SEEK_END);
    assert(offset == 1000);
    offset = lseek(fd, 0, SEEK_SET);
    assert(offset == 0);

    res = fdatasync(fd);
    assert(res == 0);

    res = fsync(fd);
    assert(res == 0);

    times[0] = (struct timespec){ .tv_sec = 1557403800, .tv_nsec = 0 };
    times[1] = (struct timespec){ .tv_sec = 1557403800, .tv_nsec = 0 };

    res = futimens(fd, times);
    assert(res == 0);

    res = pread(fd, buf, sizeof buf, 2);
    assert(res == 4);
    assert(buf[1] == 't');

    res = pwrite(fd, "T", 1, 3);
    assert(res == 1);

    res = pread(fd, buf, sizeof buf, 2);
    assert(res == 4);
    assert(buf[1] == 'T');

    res = close(fd);
    assert(res == 0);

    dir = opendir("/nonexistent");
    assert(dir == NULL);

    res = mkdir("/sandbox/test", 0755);
    assert(res == 0);

    res = mkdir("/sandbox/test", 0755);
    assert(res == -1);
    assert(errno == EEXIST);

    res = rmdir("/sandbox/test");
    assert(res == 0);

    res = rmdir("/sandbox/test");
    assert(res == -1);

    res = rename("/sandbox/testfile", "/sandbox/testfile2");
    assert(res == 0);

    res = unlink("/sandbox/testfile");
    assert(res == -1);

    res = access("/sandbox/testfile2", R_OK);
    assert(res == 0);

    res = link("/sandbox/testfile2", "/sandbox/testfile-link");
    assert(res == 0);

    res = access("/sandbox/testfile-link", R_OK);
    assert(res == 0);

    res = symlink("/sandbox/testfile-link", "/sandbox/testfile-symlink");
    assert(res == 0);

    res = symlink("/sandbox/testfile2", "/sandbox/testfile-symlink");
    assert(res == -1);

    res = sched_yield();
    assert(res == 0);

    fd = open("/sandbox/testfile2", O_RDONLY);
    assert(fd != -1);

    fp = fdopen(fd, "r");
    assert(fp != NULL);

    res = fgetc(fp);
    assert(res == 't');

    res = fclose(fp);
    assert(res == 0);

    dir = opendir("/sandbox");
    assert(dir != NULL);

    res = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            res += 1000;
        } else {
            res++;
        }
    }
    assert(res == 2003);

    res = closedir(dir);
    assert(res == 0);

    res = mkdir("/sandbox/a", 0755);
    assert(res == 0);
    res = mkdir("/sandbox/a/b", 0755);
    assert(res == 0);
    res = mkdir("/sandbox/a/b/c", 0755);
    assert(res == 0);
    res = access("/sandbox/a/b/c", R_OK);
    assert(res == 0);

    return 0;
}
