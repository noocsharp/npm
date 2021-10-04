#define PASSWORD_MAX_LEN 512
#define PASSPHRASE_MAX_LEN 512
#define KEY_LEN 32
#define NONCE_LEN 24
#define SALT_LEN 8
#define MAC_LEN 16

// Argon2 parameters
#define T_COST 250
#define M_COST 1024 // 1 mibibyte

#if SALT_LEN == 0 || SALT_LEN > SIZE_MAX - 4
#error Invalid salt size
#endif
