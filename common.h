#define PASSWORD_MAX_LEN 511
#define PASSPHRASE_MAX_LEN 512
#define KEY_LEN 32
#define NONCE_LEN 12
#define SALT_LEN 8

// Argon2 parameters
#define T_COST 250
#define M_COST 2*1024 // 2 mibibytes
#define PARALLELISM 1

#if SALT_LEN == 0 || SALT_LEN > SIZE_MAX - 4
#error Invalid salt size
#endif
