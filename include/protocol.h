#ifndef BYTES_PROTOCOL_H
#define BYTES_PROTOCOL_H

#include "common.h"

typedef enum {
    MSG_HELLO      = 1,
    MSG_WELCOME    = 2,
    MSG_GAME_START = 3,
    MSG_INPUT      = 4,
    MSG_STATE      = 5,
    MSG_GAME_OVER  = 6,
    MSG_PAUSE      = 7,
    MSG_RESUME     = 8,
    MSG_QUIT       = 9
} msg_type_t;

typedef struct {
    uint8_t  type;
    uint16_t payload_len;
} __attribute__((packed)) msg_header_t;

typedef struct {
    char    name[MAX_NAME_LEN];
    uint8_t role;
} __attribute__((packed)) msg_hello_t;

typedef struct {
    char    host_name[MAX_NAME_LEN];
    char    opponent_name[MAX_NAME_LEN];
    uint8_t assigned_id;
} __attribute__((packed)) msg_welcome_t;

typedef struct {
    uint8_t game_type;
    char    p1_name[MAX_NAME_LEN];
    char    p2_name[MAX_NAME_LEN];
} __attribute__((packed)) msg_game_start_t;

typedef struct {
    int32_t key;
} __attribute__((packed)) msg_input_t;

typedef struct {
    uint8_t winner_id;
    char    winner_name[MAX_NAME_LEN];
} __attribute__((packed)) msg_game_over_t;

typedef struct {
    uint8_t reason;
} __attribute__((packed)) msg_pause_t;

int proto_pack_header(uint8_t *buf, size_t buflen, uint8_t type, uint16_t payload_len);
int proto_pack_hello(uint8_t *buf, size_t buflen, const char *name, uint8_t role);
int proto_pack_welcome(uint8_t *buf, size_t buflen, const char *host_name,
                       const char *opponent_name, uint8_t assigned_id);
int proto_pack_game_start(uint8_t *buf, size_t buflen, uint8_t game_type,
                          const char *p1_name, const char *p2_name);
int proto_pack_input(uint8_t *buf, size_t buflen, int32_t key);
int proto_pack_state(uint8_t *buf, size_t buflen, const uint8_t *state_data, uint16_t state_len);
int proto_pack_game_over(uint8_t *buf, size_t buflen, uint8_t winner_id, const char *winner_name);
int proto_pack_pause(uint8_t *buf, size_t buflen, uint8_t reason);
int proto_pack_resume(uint8_t *buf, size_t buflen);
int proto_pack_quit(uint8_t *buf, size_t buflen);

int proto_unpack_header(const uint8_t *buf, size_t len, msg_header_t *hdr);
int proto_unpack_hello(const uint8_t *payload, size_t len, msg_hello_t *out);
int proto_unpack_welcome(const uint8_t *payload, size_t len, msg_welcome_t *out);
int proto_unpack_game_start(const uint8_t *payload, size_t len, msg_game_start_t *out);
int proto_unpack_input(const uint8_t *payload, size_t len, msg_input_t *out);
int proto_unpack_game_over(const uint8_t *payload, size_t len, msg_game_over_t *out);
int proto_unpack_pause(const uint8_t *payload, size_t len, msg_pause_t *out);

#endif
