// in bytes
#define KEY_LEN 32
#define NONCE_LEN 24
#define SALT_LEN 8
#define MAC_LEN 16

// Argon2 parameters
#define T_COST 250
#define M_COST 1024 // 1 mibibyte

// npm-agent SOCKPATH - uid is appended to the end
#ifndef SOCKPATH
#define SOCKPATH "/tmp/npm-agent"
#endif

char *const getpasscmd[] = { "bemenu", "-xindicator", "-p", "Password:", NULL };
