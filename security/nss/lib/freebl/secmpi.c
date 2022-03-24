#include "blapi.h"

#include "mpi.h"
#include "mpprime.h"

mp_err
mpp_random_secure(mp_int *a)
{
    SECStatus rv;
    rv = RNG_GenerateGlobalRandomBytes((unsigned char *)MP_DIGITS(a), MP_USED(a) * sizeof(mp_digit));
    if (rv != SECSuccess) {
        return MP_UNDEF;
    }
    MP_SIGN(a) = MP_ZPOS;
    return MP_OKAY;
}

mp_err
mpp_pprime_secure(mp_int *a, int nt)
{
    return mpp_pprime_ext_random(a, nt, &mpp_random_secure);
}

mp_err
mpp_make_prime_secure(mp_int *start, mp_size nBits, mp_size strong)
{
    return mpp_make_prime_ext_random(start, nBits, strong, &mpp_random_secure);
}
