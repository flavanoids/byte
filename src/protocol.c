#include "protocol.h"

#include <string.h>

static void write_u16_le(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
}

static uint16_t read_u16_le(const uint8_t *buf)
{
    return (uint16_t)(buf[0] | (buf[1] << 8));
}

static void write_i32_le(uint8_t *buf, int32_t val)
{
    uint32_t u;
    memcpy(&u, &val, sizeof(u));
    buf[0] = (uint8_t)(u & 0xFF);
    buf[1] = (uint8_t)((u >> 8) & 0xFF);
    buf[2] = (uint8_t)((u >> 16) & 0xFF);
    buf[3] = (uint8_t)((u >> 24) & 0xFF);
}

static int32_t read_i32_le(const uint8_t *buf)
{
    uint32_t u = (uint32_t)buf[0]
               | ((uint32_t)buf[1] << 8)
               | ((uint32_t)buf[2] << 16)
               | ((uint32_t)buf[3] << 24);
    int32_t val;
    memcpy(&val, &u, sizeof(val));
    return val;
}

static void safe_copy_name(char *dst, const char *src)
{
    strncpy(dst, src, MAX_NAME_LEN - 1);
    dst[MAX_NAME_LEN - 1] = '\0';
}

int proto_pack_header(uint8_t *buf, size_t buflen, uint8_t type, uint16_t payload_len)
{
    if (buflen < MSG_HEADER_SIZE)
        return -1;
    buf[0] = type;
    write_u16_le(buf + 1, payload_len);
    return MSG_HEADER_SIZE;
}

int proto_pack_hello(uint8_t *buf, size_t buflen, const char *name, uint8_t role)
{
    uint16_t plen = MAX_NAME_LEN + 1;
    if (buflen < (size_t)(MSG_HEADER_SIZE + plen))
        return -1;

    proto_pack_header(buf, buflen, MSG_HELLO, plen);
    uint8_t *p = buf + MSG_HEADER_SIZE;
    memset(p, 0, MAX_NAME_LEN);
    safe_copy_name((char *)p, name);
    p[MAX_NAME_LEN] = role;

    return MSG_HEADER_SIZE + plen;
}

int proto_pack_welcome(uint8_t *buf, size_t buflen, const char *host_name,
                       const char *opponent_name, uint8_t assigned_id)
{
    uint16_t plen = MAX_NAME_LEN * 2 + 1;
    if (buflen < (size_t)(MSG_HEADER_SIZE + plen))
        return -1;

    proto_pack_header(buf, buflen, MSG_WELCOME, plen);
    uint8_t *p = buf + MSG_HEADER_SIZE;
    memset(p, 0, MAX_NAME_LEN * 2);
    safe_copy_name((char *)p, host_name);
    safe_copy_name((char *)(p + MAX_NAME_LEN), opponent_name);
    p[MAX_NAME_LEN * 2] = assigned_id;

    return MSG_HEADER_SIZE + plen;
}

int proto_pack_game_start(uint8_t *buf, size_t buflen, uint8_t game_type,
                          const char *p1_name, const char *p2_name)
{
    uint16_t plen = 1 + MAX_NAME_LEN * 2;
    if (buflen < (size_t)(MSG_HEADER_SIZE + plen))
        return -1;

    proto_pack_header(buf, buflen, MSG_GAME_START, plen);
    uint8_t *p = buf + MSG_HEADER_SIZE;
    p[0] = game_type;
    memset(p + 1, 0, MAX_NAME_LEN * 2);
    safe_copy_name((char *)(p + 1), p1_name);
    safe_copy_name((char *)(p + 1 + MAX_NAME_LEN), p2_name);

    return MSG_HEADER_SIZE + plen;
}

int proto_pack_input(uint8_t *buf, size_t buflen, int32_t key)
{
    uint16_t plen = 4;
    if (buflen < (size_t)(MSG_HEADER_SIZE + plen))
        return -1;

    proto_pack_header(buf, buflen, MSG_INPUT, plen);
    write_i32_le(buf + MSG_HEADER_SIZE, key);

    return MSG_HEADER_SIZE + plen;
}

int proto_pack_state(uint8_t *buf, size_t buflen, const uint8_t *state_data, uint16_t state_len)
{
    if (buflen < (size_t)(MSG_HEADER_SIZE + state_len))
        return -1;

    proto_pack_header(buf, buflen, MSG_STATE, state_len);
    memcpy(buf + MSG_HEADER_SIZE, state_data, state_len);

    return MSG_HEADER_SIZE + state_len;
}

int proto_pack_game_over(uint8_t *buf, size_t buflen, uint8_t winner_id, const char *winner_name)
{
    uint16_t plen = 1 + MAX_NAME_LEN;
    if (buflen < (size_t)(MSG_HEADER_SIZE + plen))
        return -1;

    proto_pack_header(buf, buflen, MSG_GAME_OVER, plen);
    uint8_t *p = buf + MSG_HEADER_SIZE;
    p[0] = winner_id;
    memset(p + 1, 0, MAX_NAME_LEN);
    safe_copy_name((char *)(p + 1), winner_name);

    return MSG_HEADER_SIZE + plen;
}

int proto_pack_pause(uint8_t *buf, size_t buflen, uint8_t reason)
{
    uint16_t plen = 1;
    if (buflen < (size_t)(MSG_HEADER_SIZE + plen))
        return -1;

    proto_pack_header(buf, buflen, MSG_PAUSE, plen);
    buf[MSG_HEADER_SIZE] = reason;

    return MSG_HEADER_SIZE + plen;
}

int proto_pack_resume(uint8_t *buf, size_t buflen)
{
    if (buflen < MSG_HEADER_SIZE)
        return -1;
    return proto_pack_header(buf, buflen, MSG_RESUME, 0);
}

int proto_pack_quit(uint8_t *buf, size_t buflen)
{
    if (buflen < MSG_HEADER_SIZE)
        return -1;
    return proto_pack_header(buf, buflen, MSG_QUIT, 0);
}

int proto_unpack_header(const uint8_t *buf, size_t len, msg_header_t *hdr)
{
    if (len < MSG_HEADER_SIZE)
        return -1;
    hdr->type = buf[0];
    hdr->payload_len = read_u16_le(buf + 1);
    return 0;
}

int proto_unpack_hello(const uint8_t *payload, size_t len, msg_hello_t *out)
{
    if (len < (size_t)(MAX_NAME_LEN + 1))
        return -1;
    memset(out, 0, sizeof(*out));
    memcpy(out->name, payload, MAX_NAME_LEN);
    out->name[MAX_NAME_LEN - 1] = '\0';
    out->role = payload[MAX_NAME_LEN];
    return 0;
}

int proto_unpack_welcome(const uint8_t *payload, size_t len, msg_welcome_t *out)
{
    if (len < (size_t)(MAX_NAME_LEN * 2 + 1))
        return -1;
    memset(out, 0, sizeof(*out));
    memcpy(out->host_name, payload, MAX_NAME_LEN);
    out->host_name[MAX_NAME_LEN - 1] = '\0';
    memcpy(out->opponent_name, payload + MAX_NAME_LEN, MAX_NAME_LEN);
    out->opponent_name[MAX_NAME_LEN - 1] = '\0';
    out->assigned_id = payload[MAX_NAME_LEN * 2];
    return 0;
}

int proto_unpack_game_start(const uint8_t *payload, size_t len, msg_game_start_t *out)
{
    if (len < (size_t)(1 + MAX_NAME_LEN * 2))
        return -1;
    memset(out, 0, sizeof(*out));
    out->game_type = payload[0];
    memcpy(out->p1_name, payload + 1, MAX_NAME_LEN);
    out->p1_name[MAX_NAME_LEN - 1] = '\0';
    memcpy(out->p2_name, payload + 1 + MAX_NAME_LEN, MAX_NAME_LEN);
    out->p2_name[MAX_NAME_LEN - 1] = '\0';
    return 0;
}

int proto_unpack_input(const uint8_t *payload, size_t len, msg_input_t *out)
{
    if (len < 4)
        return -1;
    out->key = read_i32_le(payload);
    return 0;
}

int proto_unpack_game_over(const uint8_t *payload, size_t len, msg_game_over_t *out)
{
    if (len < (size_t)(1 + MAX_NAME_LEN))
        return -1;
    memset(out, 0, sizeof(*out));
    out->winner_id = payload[0];
    memcpy(out->winner_name, payload + 1, MAX_NAME_LEN);
    out->winner_name[MAX_NAME_LEN - 1] = '\0';
    return 0;
}

int proto_unpack_pause(const uint8_t *payload, size_t len, msg_pause_t *out)
{
    if (len < 1)
        return -1;
    out->reason = payload[0];
    return 0;
}
