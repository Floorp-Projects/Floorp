#define FOO 1
#define BAR 4
#define BAZ (FOO + BAR)

#define MIN (1 << 63)

#define BARR (1 << 0)
#define BAZZ ((1 << 1) + BAZ)
#define I_RAN_OUT_OF_DUMB_NAMES (BARR | BAZZ)

/* I haz a comment */
#define HAZ_A_COMMENT BARR

#define HAZ_A_COMMENT_INSIDE (/* comment for real */ BARR + FOO)
