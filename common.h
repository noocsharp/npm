#define PASSWORD_MAX_LEN 512
#define PASSPHRASE_MAX_LEN 512
#define KEY_LEN 32
#define NONCE_LEN 12
#define SALT_LEN 8
#define ROUNDS 2000
#define DEFAULT_LEN 40

#if SALT_LEN == 0 || SALT_LEN > SIZE_MAX - 4
#error Invalid salt size
#endif
