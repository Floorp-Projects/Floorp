#include <signal.h>

void *siginfo_si_addr(siginfo_t *si)
{
    return si->si_addr;
}
