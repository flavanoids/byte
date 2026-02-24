#include "game.h"
#include "protocol.h"
#include "ui.h"
#include "pong.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const game_def_t *game_registry[] = {
    &pong_game_def
};
#define GAME_REGISTRY_COUNT ((int)(sizeof(game_registry) / sizeof(game_registry[0])))

const game_def_t *game_get_def(game_type_t type)
{
    for (int i = 0; i < GAME_REGISTRY_COUNT; i++) {
        if (game_registry[i]->type == type)
            return game_registry[i];
    }
    return NULL;
}

int game_get_count(void)
{
    return GAME_REGISTRY_COUNT;
}

const char *game_get_name(int index)
{
    if (index < 0 || index >= GAME_REGISTRY_COUNT)
        return NULL;
    return game_registry[index]->name;
}

void game_session_init(game_session_t *gs, const game_def_t *def,
                       const char *p1, const char *p2,
                       bool is_server, bool is_spectator,
                       uint8_t local_player_id)
{
    memset(gs, 0, sizeof(*gs));
    gs->def = def;
    strncpy(gs->p1_name, p1, MAX_NAME_LEN - 1);
    strncpy(gs->p2_name, p2, MAX_NAME_LEN - 1);
    gs->is_server = is_server;
    gs->is_spectator = is_spectator;
    gs->local_player_id = local_player_id;
    gs->running = true;
    gs->paused = false;

    gs->state = calloc(1, def->state_size);

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    def->init(gs->state, rows, cols);
}

void game_session_cleanup(game_session_t *gs)
{
    free(gs->state);
    gs->state = NULL;
}

static int64_t time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

void game_run_server(game_session_t *gs, net_server_t *srv, int player_client_idx)
{
    const game_def_t *def = gs->def;
    uint8_t send_buf[MSG_HEADER_SIZE + MAX_MSG_PAYLOAD];
    uint8_t recv_buf[MSG_HEADER_SIZE + MAX_MSG_PAYLOAD];
    uint8_t state_buf[MAX_MSG_PAYLOAD];

    keypad(stdscr, TRUE);
    timeout(TICK_INTERVAL_US / 1000 / 2);

    int64_t last_tick = time_us();
    int reconnect_countdown = 0;
    int64_t disconnect_time = 0;

    while (gs->running) {
        int64_t now = time_us();

        /* Handle pause / reconnect timeout */
        if (gs->paused) {
            int elapsed = (int)((now - disconnect_time) / 1000000);
            int remaining = RECONNECT_TIMEOUT_SEC - elapsed;
            if (remaining <= 0) {
                /* Forfeit: the remaining player wins */
                int winner = (player_client_idx >= 0) ? 2 : 1;
                const char *wname = (winner == 1) ? gs->p1_name : gs->p2_name;

                int n = proto_pack_game_over(send_buf, sizeof(send_buf),
                                             (uint8_t)winner, wname);
                if (n > 0)
                    net_send_to_all(srv, send_buf, (size_t)n);

                gs->running = false;
                bool you_won = (winner == 1);
                ui_game_over(wname, you_won);
                break;
            }

            /* Check for reconnect */
            int new_idx = net_server_accept(srv, 100);
            if (new_idx >= 0) {
                int rr = net_recv(srv->clients[new_idx].fd, recv_buf,
                                  sizeof(recv_buf), 3000);
                if (rr > 0) {
                    msg_header_t hdr;
                    proto_unpack_header(recv_buf, (size_t)rr, &hdr);
                    if (hdr.type == MSG_HELLO) {
                        msg_hello_t hello;
                        proto_unpack_hello(recv_buf + MSG_HEADER_SIZE,
                                           hdr.payload_len, &hello);
                        if (hello.role == ROLE_PLAYER) {
                            srv->clients[new_idx].is_player = true;
                            srv->clients[new_idx].player_id = 2;
                            strncpy(srv->clients[new_idx].name, hello.name,
                                    MAX_NAME_LEN - 1);
                            player_client_idx = new_idx;

                            int wn = proto_pack_welcome(send_buf, sizeof(send_buf),
                                                        gs->p1_name, gs->p2_name, 2);
                            if (wn > 0)
                                net_send(srv->clients[new_idx].fd, send_buf, (size_t)wn, 500);

                            int rn = proto_pack_resume(send_buf, sizeof(send_buf));
                            if (rn > 0)
                                net_send_to_all(srv, send_buf, (size_t)rn);

                            gs->paused = false;
                            reconnect_countdown = 0;
                        } else {
                            srv->clients[new_idx].is_player = false;
                            gs->spectator_count++;
                        }
                    }
                }
            }

            ui_pause_overlay(remaining);
            continue;
        }

        /* Accept new spectators during gameplay */
        int spec_idx = net_server_accept(srv, 0);
        if (spec_idx >= 0) {
            int rr = net_recv(srv->clients[spec_idx].fd, recv_buf,
                              sizeof(recv_buf), 1000);
            if (rr > 0) {
                msg_header_t hdr;
                proto_unpack_header(recv_buf, (size_t)rr, &hdr);
                if (hdr.type == MSG_HELLO) {
                    msg_hello_t hello;
                    proto_unpack_hello(recv_buf + MSG_HEADER_SIZE,
                                       hdr.payload_len, &hello);
                    srv->clients[spec_idx].is_player = false;
                    strncpy(srv->clients[spec_idx].name, hello.name,
                            MAX_NAME_LEN - 1);

                    int wn = proto_pack_welcome(send_buf, sizeof(send_buf),
                                                gs->p1_name, gs->p2_name, 0);
                    if (wn > 0)
                        net_send(srv->clients[spec_idx].fd, send_buf, (size_t)wn, 500);

                    int gn = proto_pack_game_start(send_buf, sizeof(send_buf),
                                                   (uint8_t)def->type,
                                                   gs->p1_name, gs->p2_name);
                    if (gn > 0)
                        net_send(srv->clients[spec_idx].fd, send_buf, (size_t)gn, 500);

                    gs->spectator_count++;
                }
            }
        }

        /* Read remote player input */
        if (player_client_idx >= 0 && srv->clients[player_client_idx].connected) {
            int pr = net_poll_readable(srv->clients[player_client_idx].fd, 0);
            if (pr > 0) {
                int rr = net_recv(srv->clients[player_client_idx].fd,
                                  recv_buf, sizeof(recv_buf), 0);
                if (rr > 0) {
                    msg_header_t hdr;
                    proto_unpack_header(recv_buf, (size_t)rr, &hdr);
                    if (hdr.type == MSG_INPUT) {
                        msg_input_t inp;
                        proto_unpack_input(recv_buf + MSG_HEADER_SIZE,
                                           hdr.payload_len, &inp);
                        def->handle_input(gs->state, 2, inp.key);
                    } else if (hdr.type == MSG_QUIT) {
                        gs->running = false;
                        break;
                    }
                } else if (rr < 0) {
                    /* Player disconnected */
                    net_server_close_client(srv, player_client_idx);
                    player_client_idx = -1;
                    gs->paused = true;
                    disconnect_time = now;
                    reconnect_countdown = RECONNECT_TIMEOUT_SEC;

                    int pn = proto_pack_pause(send_buf, sizeof(send_buf), 0);
                    if (pn > 0)
                        net_send_to_all(srv, send_buf, (size_t)pn);

                    (void)reconnect_countdown;
                    continue;
                }
            } else if (pr < 0) {
                net_server_close_client(srv, player_client_idx);
                player_client_idx = -1;
                gs->paused = true;
                disconnect_time = now;

                int pn = proto_pack_pause(send_buf, sizeof(send_buf), 0);
                if (pn > 0)
                    net_send_to_all(srv, send_buf, (size_t)pn);
                continue;
            }
        }

        /* Read local input (player 1 = server host) */
        int ch = getch();
        if (ch == 'q') {
            int qn = proto_pack_quit(send_buf, sizeof(send_buf));
            if (qn > 0)
                net_send_to_all(srv, send_buf, (size_t)qn);
            gs->running = false;
            break;
        }
        if (ch != ERR)
            def->handle_input(gs->state, 1, ch);

        /* Tick at fixed rate */
        if (now - last_tick >= TICK_INTERVAL_US) {
            last_tick = now;

            def->update(gs->state);

            /* Render locally */
            clear();
            def->render(gs->state, gs->p1_name, gs->p2_name,
                        false, gs->spectator_count);
            refresh();

            /* Broadcast state to all clients */
            int slen = def->pack_state(gs->state, state_buf, sizeof(state_buf));
            if (slen > 0) {
                int pkt = proto_pack_state(send_buf, sizeof(send_buf),
                                           state_buf, (uint16_t)slen);
                if (pkt > 0)
                    net_send_to_all(srv, send_buf, (size_t)pkt);
            }

            /* Check game over */
            if (def->is_over(gs->state)) {
                int winner = def->get_winner(gs->state);
                const char *wname = (winner == 1) ? gs->p1_name : gs->p2_name;

                int gon = proto_pack_game_over(send_buf, sizeof(send_buf),
                                               (uint8_t)winner, wname);
                if (gon > 0)
                    net_send_to_all(srv, send_buf, (size_t)gon);

                gs->running = false;
                bool you_won = (winner == 1);
                ui_game_over(wname, you_won);
                break;
            }
        }

        /* Sleep to avoid busy-spinning */
        int64_t elapsed = time_us() - now;
        int64_t remaining_us = TICK_INTERVAL_US - elapsed;
        if (remaining_us > 1000)
            usleep((unsigned)(remaining_us / 2));
    }

    nodelay(stdscr, FALSE);
}

void game_run_client(game_session_t *gs, net_connection_t *conn)
{
    const game_def_t *def = gs->def;
    uint8_t send_buf[MSG_HEADER_SIZE + MAX_MSG_PAYLOAD];
    uint8_t recv_buf[MSG_HEADER_SIZE + MAX_MSG_PAYLOAD];

    keypad(stdscr, TRUE);
    timeout(TICK_INTERVAL_US / 1000 / 2);

    while (gs->running) {
        /* Read local input, send to server */
        int ch = getch();
        if (ch == 'q') {
            int qn = proto_pack_quit(send_buf, sizeof(send_buf));
            if (qn > 0)
                net_send(conn->fd, send_buf, (size_t)qn, 500);
            gs->running = false;
            break;
        }
        if (ch != ERR) {
            int n = proto_pack_input(send_buf, sizeof(send_buf), ch);
            if (n > 0)
                net_send(conn->fd, send_buf, (size_t)n, 100);
        }

        /* Drain all pending messages, render only the latest state */
        bool got_state = false;
        for (;;) {
            int rr = net_recv(conn->fd, recv_buf, sizeof(recv_buf), got_state ? 0 : 10);
            if (rr <= 0) {
                if (rr < 0) {
                    gs->running = false;
                    ui_show_message("Connection to server lost.");
                    nodelay(stdscr, FALSE);
                    getch();
                }
                break;
            }

            msg_header_t hdr;
            proto_unpack_header(recv_buf, (size_t)rr, &hdr);

            switch (hdr.type) {
            case MSG_STATE:
                def->unpack_state(gs->state, recv_buf + MSG_HEADER_SIZE,
                                  hdr.payload_len);
                got_state = true;
                break;
            case MSG_GAME_OVER: {
                msg_game_over_t go;
                proto_unpack_game_over(recv_buf + MSG_HEADER_SIZE,
                                       hdr.payload_len, &go);
                gs->running = false;
                bool you_won = (go.winner_id == gs->local_player_id);
                ui_game_over(go.winner_name, you_won);
                break;
            }
            case MSG_PAUSE:
                gs->paused = true;
                break;
            case MSG_RESUME:
                gs->paused = false;
                break;
            case MSG_QUIT:
                gs->running = false;
                break;
            default:
                break;
            }

            if (!gs->running)
                break;
        }

        if (got_state && gs->running) {
            clear();
            def->render(gs->state, gs->p1_name, gs->p2_name,
                        false, gs->spectator_count);
            refresh();
        }

        if (gs->paused) {
            ui_pause_overlay(RECONNECT_TIMEOUT_SEC);
            usleep(500000);
        }
    }

    nodelay(stdscr, FALSE);
}

void game_run_spectator(game_session_t *gs, net_connection_t *conn)
{
    const game_def_t *def = gs->def;
    uint8_t recv_buf[MSG_HEADER_SIZE + MAX_MSG_PAYLOAD];

    keypad(stdscr, TRUE);
    timeout(TICK_INTERVAL_US / 1000 / 2);

    while (gs->running) {
        int ch = getch();
        if (ch == 'q') {
            gs->running = false;
            break;
        }

        bool got_state = false;
        for (;;) {
            int rr = net_recv(conn->fd, recv_buf, sizeof(recv_buf), got_state ? 0 : 30);
            if (rr <= 0) {
                if (rr < 0) {
                    gs->running = false;
                    ui_show_message("Connection to server lost.");
                    nodelay(stdscr, FALSE);
                    getch();
                }
                break;
            }

            msg_header_t hdr;
            proto_unpack_header(recv_buf, (size_t)rr, &hdr);

            switch (hdr.type) {
            case MSG_STATE:
                def->unpack_state(gs->state, recv_buf + MSG_HEADER_SIZE,
                                  hdr.payload_len);
                got_state = true;
                break;
            case MSG_GAME_OVER: {
                msg_game_over_t go;
                proto_unpack_game_over(recv_buf + MSG_HEADER_SIZE,
                                       hdr.payload_len, &go);
                gs->running = false;
                ui_game_over(go.winner_name, false);
                break;
            }
            case MSG_PAUSE:
                gs->paused = true;
                break;
            case MSG_RESUME:
                gs->paused = false;
                break;
            case MSG_QUIT:
                gs->running = false;
                break;
            default:
                break;
            }

            if (!gs->running)
                break;
        }

        if (got_state && gs->running) {
            clear();
            def->render(gs->state, gs->p1_name, gs->p2_name,
                        true, gs->spectator_count);
            refresh();
        }

        if (gs->paused)
            ui_pause_overlay(RECONNECT_TIMEOUT_SEC);
    }

    nodelay(stdscr, FALSE);
}
