#include "common.h"
#include "game.h"
#include "network.h"
#include "pong.h"
#include "protocol.h"
#include "stats.h"
#include "ui.h"
#include "platform.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef BYTES_WINDOWS
#include <signal.h>
#endif

static volatile int g_quit = 0;

#ifdef BYTES_WINDOWS
static BOOL WINAPI console_handler(DWORD sig)
{
    (void)sig;
    g_quit = 1;
    return TRUE;
}
#else
static void handle_sigint(int sig)
{
    (void)sig;
    g_quit = 1;
}
#endif

static int parse_port(int argc, char **argv, int default_port)
{
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) {
            int p = atoi(argv[i + 1]);
            if (p > 0 && p < 65536)
                return p;
        }
    }
    return default_port;
}

static bool parse_flag(int argc, char **argv, const char *flag)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], flag) == 0)
            return true;
    }
    return false;
}

/* ── Key Input Tester ────────────────────────────────────────────── */

#define TEST_HIST_MAX 16

typedef struct {
    bool up, down, left, right;
    bool w, s, enter, esc;
} seen_keys_t;

static const char *key_label(int ch)
{
    switch (ch) {
    case KEY_UP:    return "UP";
    case KEY_DOWN:  return "DOWN";
    case KEY_LEFT:  return "LEFT";
    case KEY_RIGHT: return "RIGHT";
    case '\n': case '\r': return "ENTER";
    case 27:   return "ESC";
    default:   return keyname(ch);
    }
}

static void draw_test_screen(int last_key, const seen_keys_t *seen,
                              int *hist, int hist_count)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows;
    clear();

    int cx = (cols - 44) / 2;
    if (cx < 1) cx = 1;
    int cy = 2;

    attron(COLOR_PAIR(COLOR_MENU) | A_BOLD);
    mvaddstr(cy, (cols - 18) / 2, "Key Input Tester");
    attroff(COLOR_PAIR(COLOR_MENU) | A_BOLD);
    cy += 2;

    attron(COLOR_PAIR(COLOR_DIM) | A_DIM);
    mvaddstr(cy, (cols - 40) / 2, "Press keys to test. Press q to exit.");
    attroff(COLOR_PAIR(COLOR_DIM) | A_DIM);
    cy += 2;

    if (last_key != ERR) {
        attron(COLOR_PAIR(COLOR_BORDER) | A_BOLD);
        mvprintw(cy, cx, "Last key:  ");
        attroff(COLOR_PAIR(COLOR_BORDER) | A_BOLD);
        attron(COLOR_PAIR(COLOR_BALL) | A_BOLD);
        printw("%-12s", key_label(last_key));
        attroff(COLOR_PAIR(COLOR_BALL) | A_BOLD);
        attron(COLOR_PAIR(COLOR_DIM));
        printw("(code: %d)", last_key);
        attroff(COLOR_PAIR(COLOR_DIM));
    }
    cy += 2;

    int bw = 38;
    int bh = 12;
    int bx = (cols - bw) / 2;
    ui_draw_box(cy, bx, bh, bw);

    attron(COLOR_PAIR(COLOR_BORDER) | A_BOLD);
    mvaddstr(cy, bx + (bw - 18) / 2, " Game Controls ");
    attroff(COLOR_PAIR(COLOR_BORDER) | A_BOLD);

    struct { const char *label; bool detected; } checks[] = {
        { "Arrow UP",    seen->up    },
        { "Arrow DOWN",  seen->down  },
        { "Arrow LEFT",  seen->left  },
        { "Arrow RIGHT", seen->right },
        { "w",           seen->w     },
        { "s",           seen->s     },
        { "Enter",       seen->enter },
        { "Escape",      seen->esc   },
    };
    int nck = (int)(sizeof(checks) / sizeof(checks[0]));

    for (int i = 0; i < nck; i++) {
        int y = cy + 1 + i;
        if (checks[i].detected) {
            attron(COLOR_PAIR(COLOR_MENU) | A_BOLD);
            mvprintw(y, bx + 3, "\u2714  %-14s", checks[i].label);
            attroff(COLOR_PAIR(COLOR_MENU) | A_BOLD);
            attron(COLOR_PAIR(COLOR_MENU));
            printw("detected");
            attroff(COLOR_PAIR(COLOR_MENU));
        } else {
            attron(COLOR_PAIR(COLOR_DIM) | A_DIM);
            mvprintw(y, bx + 3, "\u00B7  %-14s", checks[i].label);
            printw("waiting");
            attroff(COLOR_PAIR(COLOR_DIM) | A_DIM);
        }
    }

    attron(COLOR_PAIR(COLOR_BORDER));
    mvprintw(cy + nck + 1, bx + 3, "Total detected: ");
    int total = 0;
    for (int i = 0; i < nck; i++)
        total += checks[i].detected ? 1 : 0;
    if (total == nck) {
        attron(COLOR_PAIR(COLOR_MENU) | A_BOLD);
        printw("%d/%d  ALL GOOD", total, nck);
        attroff(COLOR_PAIR(COLOR_MENU) | A_BOLD);
    } else {
        printw("%d/%d", total, nck);
    }
    attroff(COLOR_PAIR(COLOR_BORDER));

    cy += bh + 1;

    if (hist_count > 0) {
        attron(COLOR_PAIR(COLOR_BORDER) | A_BOLD);
        mvaddstr(cy, bx, "History: ");
        attroff(COLOR_PAIR(COLOR_BORDER) | A_BOLD);

        attron(COLOR_PAIR(COLOR_DIM));
        int start = hist_count > 8 ? hist_count - 8 : 0;
        for (int i = start; i < hist_count; i++) {
            printw("%s ", key_label(hist[i]));
        }
        attroff(COLOR_PAIR(COLOR_DIM));
    }

    refresh();
}

static void run_test_keys(void)
{
    keypad(stdscr, TRUE);
    nodelay(stdscr, FALSE);
#ifdef ESCDELAY
    ESCDELAY = 10;
#endif

    seen_keys_t seen;
    memset(&seen, 0, sizeof(seen));

    int hist[TEST_HIST_MAX];
    int hist_count = 0;
    int last_key = ERR;

    draw_test_screen(last_key, &seen, hist, hist_count);

    while (!g_quit) {
        int ch = getch();
        if (ch == ERR)
            continue;

        last_key = ch;

        switch (ch) {
        case KEY_UP:    seen.up    = true; break;
        case KEY_DOWN:  seen.down  = true; break;
        case KEY_LEFT:  seen.left  = true; break;
        case KEY_RIGHT: seen.right = true; break;
        case 'w': case 'W': seen.w = true; break;
        case 's': case 'S': seen.s = true; break;
        case '\n': case '\r': seen.enter = true; break;
        case 27:  seen.esc = true; break;
        default: break;
        }

        if (hist_count < TEST_HIST_MAX) {
            hist[hist_count++] = ch;
        } else {
            memmove(hist, hist + 1, sizeof(int) * (TEST_HIST_MAX - 1));
            hist[TEST_HIST_MAX - 1] = ch;
        }

        draw_test_screen(last_key, &seen, hist, hist_count);

        if (ch == 'q') {
            platform_usleep(600000);
            break;
        }
    }
}

/* ── Solo Play (local Pong vs CPU) ───────────────────────────────── */

static void run_solo(stats_t *st)
{
    const game_def_t *def = game_get_def(GAME_PONG);
    game_session_t gs;
    game_session_init(&gs, def, "You", "CPU", true, false, 1);

    keypad(stdscr, TRUE);
#ifdef ESCDELAY
    ESCDELAY = 10;
#endif

    ui_countdown("You", "CPU");

    int64_t last_tick = platform_mono_us();
    int ai_tick = 0;

    while (gs.running && !g_quit) {
        int64_t now = platform_mono_us();

        timeout(TICK_INTERVAL_US / 1000 / 2);
        int ch = getch();
        if (ch == 'q') {
            gs.running = false;
            break;
        }
        if (ch != ERR)
            def->handle_input(gs.state, 1, ch);

        if (now - last_tick >= TICK_INTERVAL_US) {
            last_tick = now;

            pong_state_t *ps = (pong_state_t *)gs.state;
            if (ai_tick % 3 == 0) {
                float ball_y = ps->ball.y;
                float pad_mid = (float)ps->p2.y + (float)ps->p2.len / 2.0f;
                if (ball_y < pad_mid - 1.0f)
                    def->handle_input(gs.state, 2, KEY_UP);
                else if (ball_y > pad_mid + 1.0f)
                    def->handle_input(gs.state, 2, KEY_DOWN);
            }
            ai_tick++;

            def->update(gs.state);

            clear();
            def->render(gs.state, gs.p1_name, gs.p2_name, false, 0);
            refresh();

            if (def->is_over(gs.state)) {
                gs.running = false;
                int winner = def->get_winner(gs.state);
                const char *wname = (winner == 1) ? gs.p1_name : gs.p2_name;

                if (st != NULL)
                    stats_record_game(st, "pong", winner == 1);

                ui_game_over(wname, winner == 1);
                break;
            }
        }

        int64_t elapsed = platform_mono_us() - now;
        int64_t remaining_us = TICK_INTERVAL_US - elapsed;
        if (remaining_us > 1000)
            platform_usleep((unsigned)(remaining_us / 2));
    }

    nodelay(stdscr, FALSE);
    game_session_cleanup(&gs);
}

static void run_host(int port, stats_t *st)
{
    char my_name[MAX_NAME_LEN];
    ui_get_name(my_name, sizeof(my_name));

    net_server_t srv;
    if (net_server_init(&srv, port) < 0) {
        ui_show_message("Failed to start server. Port may be in use.");
        nodelay(stdscr, FALSE);
        getch();
        return;
    }

    char ip[64];
    net_get_local_ip(ip, sizeof(ip));
    ui_waiting_screen(my_name, ip, port);

    int player_idx = -1;
    nodelay(stdscr, TRUE);
    while (player_idx < 0 && !g_quit) {
        int ch = getch();
        if (ch == 'q' || ch == 27) {
            net_server_shutdown(&srv);
            nodelay(stdscr, FALSE);
            return;
        }

        player_idx = net_server_accept(&srv, 200);
    }

    if (g_quit || player_idx < 0) {
        net_server_shutdown(&srv);
        nodelay(stdscr, FALSE);
        return;
    }
    nodelay(stdscr, FALSE);

    uint8_t recv_buf[MSG_HEADER_SIZE + MAX_MSG_PAYLOAD];
    int rr = net_recv(srv.clients[player_idx].fd, recv_buf, sizeof(recv_buf), 5000);
    if (rr <= 0) {
        net_server_shutdown(&srv);
        ui_show_message("Player failed to send hello.");
        getch();
        return;
    }

    msg_header_t hdr;
    proto_unpack_header(recv_buf, (size_t)rr, &hdr);
    if (hdr.type != MSG_HELLO) {
        net_server_shutdown(&srv);
        ui_show_message("Unexpected message from client.");
        getch();
        return;
    }

    msg_hello_t hello;
    proto_unpack_hello(recv_buf + MSG_HEADER_SIZE, hdr.payload_len, &hello);

    char peer_name[MAX_NAME_LEN];
    strncpy(peer_name, hello.name, MAX_NAME_LEN - 1);
    peer_name[MAX_NAME_LEN - 1] = '\0';

    srv.clients[player_idx].is_player = true;
    srv.clients[player_idx].player_id = 2;
    strncpy(srv.clients[player_idx].name, peer_name, MAX_NAME_LEN - 1);

    uint8_t send_buf[MSG_HEADER_SIZE + MAX_MSG_PAYLOAD];
    int wn = proto_pack_welcome(send_buf, sizeof(send_buf), my_name, peer_name, 2);
    if (wn > 0)
        net_send(srv.clients[player_idx].fd, send_buf, (size_t)wn, 1000);

    ui_player_joined(my_name, peer_name);

    int gn = proto_pack_game_start(send_buf, sizeof(send_buf),
                                   GAME_PONG, my_name, peer_name);
    if (gn > 0)
        net_send_to_all(&srv, send_buf, (size_t)gn);

    ui_countdown(my_name, peer_name);

    const game_def_t *def = game_get_def(GAME_PONG);
    game_session_t gs;
    game_session_init(&gs, def, my_name, peer_name, true, false, 1);
    game_run_server(&gs, &srv, player_idx);

    if (def->is_over(gs.state)) {
        int winner = def->get_winner(gs.state);
        stats_record_game(st, "pong", winner == 1);
    }

    game_session_cleanup(&gs);
    net_server_shutdown(&srv);
}

static void run_join(int default_port, stats_t *st)
{
    char my_name[MAX_NAME_LEN];
    ui_get_name(my_name, sizeof(my_name));

    char host[64];
    int port = default_port;
    ui_get_host_and_port(host, sizeof(host), &port, default_port);

    ui_show_message("Connecting...");

    net_connection_t conn;
    if (net_client_connect(&conn, host, port) < 0) {
        ui_show_message("Failed to connect to server.");
        nodelay(stdscr, FALSE);
        getch();
        return;
    }

    uint8_t send_buf[MSG_HEADER_SIZE + MAX_MSG_PAYLOAD];
    int hn = proto_pack_hello(send_buf, sizeof(send_buf), my_name, ROLE_PLAYER);
    if (hn > 0)
        net_send(conn.fd, send_buf, (size_t)hn, 1000);

    uint8_t recv_buf[MSG_HEADER_SIZE + MAX_MSG_PAYLOAD];
    int rr = net_recv(conn.fd, recv_buf, sizeof(recv_buf), 5000);
    if (rr <= 0) {
        net_client_disconnect(&conn);
        ui_show_message("No response from server.");
        nodelay(stdscr, FALSE);
        getch();
        return;
    }

    msg_header_t hdr;
    proto_unpack_header(recv_buf, (size_t)rr, &hdr);
    if (hdr.type != MSG_WELCOME) {
        net_client_disconnect(&conn);
        ui_show_message("Unexpected response from server.");
        nodelay(stdscr, FALSE);
        getch();
        return;
    }

    msg_welcome_t welcome;
    proto_unpack_welcome(recv_buf + MSG_HEADER_SIZE, hdr.payload_len, &welcome);

    char host_name[MAX_NAME_LEN];
    strncpy(host_name, welcome.host_name, MAX_NAME_LEN - 1);
    host_name[MAX_NAME_LEN - 1] = '\0';
    uint8_t my_id = welcome.assigned_id;

    ui_player_joined(host_name, my_name);

    rr = net_recv(conn.fd, recv_buf, sizeof(recv_buf), 10000);
    if (rr <= 0) {
        net_client_disconnect(&conn);
        ui_show_message("Timed out waiting for game start.");
        nodelay(stdscr, FALSE);
        getch();
        return;
    }

    proto_unpack_header(recv_buf, (size_t)rr, &hdr);
    if (hdr.type != MSG_GAME_START) {
        net_client_disconnect(&conn);
        ui_show_message("Unexpected message.");
        nodelay(stdscr, FALSE);
        getch();
        return;
    }

    msg_game_start_t gs_msg;
    proto_unpack_game_start(recv_buf + MSG_HEADER_SIZE, hdr.payload_len, &gs_msg);

    ui_countdown(gs_msg.p1_name, gs_msg.p2_name);

    const game_def_t *def = game_get_def((game_type_t)gs_msg.game_type);
    if (def == NULL) {
        net_client_disconnect(&conn);
        ui_show_message("Unknown game type.");
        nodelay(stdscr, FALSE);
        getch();
        return;
    }

    game_session_t gs;
    game_session_init(&gs, def, gs_msg.p1_name, gs_msg.p2_name, false, false, my_id);
    game_run_client(&gs, &conn);

    if (def->is_over(gs.state)) {
        int winner = def->get_winner(gs.state);
        stats_record_game(st, "pong", winner == (int)my_id);
    }

    game_session_cleanup(&gs);
    net_client_disconnect(&conn);
}

static void run_watch(int default_port)
{
    char my_name[MAX_NAME_LEN];
    ui_get_name(my_name, sizeof(my_name));

    char host[64];
    int port = default_port;
    ui_get_host_and_port(host, sizeof(host), &port, default_port);

    ui_show_message("Connecting as spectator...");

    net_connection_t conn;
    if (net_client_connect(&conn, host, port) < 0) {
        ui_show_message("Failed to connect to server.");
        nodelay(stdscr, FALSE);
        getch();
        return;
    }

    uint8_t send_buf[MSG_HEADER_SIZE + MAX_MSG_PAYLOAD];
    int hn = proto_pack_hello(send_buf, sizeof(send_buf), my_name, ROLE_SPECTATOR);
    if (hn > 0)
        net_send(conn.fd, send_buf, (size_t)hn, 1000);

    uint8_t recv_buf[MSG_HEADER_SIZE + MAX_MSG_PAYLOAD];
    int rr = net_recv(conn.fd, recv_buf, sizeof(recv_buf), 5000);
    if (rr <= 0) {
        net_client_disconnect(&conn);
        ui_show_message("No response from server.");
        nodelay(stdscr, FALSE);
        getch();
        return;
    }

    msg_header_t hdr;
    proto_unpack_header(recv_buf, (size_t)rr, &hdr);
    if (hdr.type != MSG_WELCOME) {
        net_client_disconnect(&conn);
        ui_show_message("Unexpected response.");
        nodelay(stdscr, FALSE);
        getch();
        return;
    }

    msg_welcome_t welcome;
    proto_unpack_welcome(recv_buf + MSG_HEADER_SIZE, hdr.payload_len, &welcome);

    rr = net_recv(conn.fd, recv_buf, sizeof(recv_buf), 30000);
    if (rr <= 0) {
        net_client_disconnect(&conn);
        ui_show_message("Timed out waiting for game.");
        nodelay(stdscr, FALSE);
        getch();
        return;
    }

    proto_unpack_header(recv_buf, (size_t)rr, &hdr);
    if (hdr.type != MSG_GAME_START) {
        net_client_disconnect(&conn);
        ui_show_message("Unexpected message.");
        nodelay(stdscr, FALSE);
        getch();
        return;
    }

    msg_game_start_t gs_msg;
    proto_unpack_game_start(recv_buf + MSG_HEADER_SIZE, hdr.payload_len, &gs_msg);

    const game_def_t *def = game_get_def((game_type_t)gs_msg.game_type);
    if (def == NULL) {
        net_client_disconnect(&conn);
        ui_show_message("Unknown game type.");
        nodelay(stdscr, FALSE);
        getch();
        return;
    }

    game_session_t gs;
    game_session_init(&gs, def, gs_msg.p1_name, gs_msg.p2_name, false, true, 0);
    game_run_spectator(&gs, &conn);

    game_session_cleanup(&gs);
    net_client_disconnect(&conn);
}

int main(int argc, char **argv)
{
#ifdef BYTES_WINDOWS
    SetConsoleCtrlHandler(console_handler, TRUE);
#else
    signal(SIGINT, handle_sigint);
#endif

    platform_ignore_sigpipe();
    platform_net_init();

    int port = parse_port(argc, argv, DEFAULT_PORT);
    bool flag_test_keys = parse_flag(argc, argv, "--test-keys");
    bool flag_solo = parse_flag(argc, argv, "--solo");

    stats_t stats;
    stats_init(&stats);
    stats_load(&stats);

    ui_init();

    if (flag_test_keys) {
        run_test_keys();
        ui_cleanup();
        platform_net_cleanup();
        return 0;
    }

    if (flag_solo) {
        run_solo(&stats);
        ui_cleanup();
        platform_net_cleanup();
        return 0;
    }

    while (!g_quit) {
        menu_choice_t choice = ui_main_menu();

        switch (choice) {
        case MENU_HOST:
            run_host(port, &stats);
            break;
        case MENU_JOIN:
            run_join(port, &stats);
            break;
        case MENU_SOLO:
            run_solo(&stats);
            break;
        case MENU_WATCH:
            run_watch(port);
            break;
        case MENU_STATS:
            stats_display(&stats);
            break;
        case MENU_QUIT:
            g_quit = 1;
            break;
        default:
            break;
        }
    }

    ui_cleanup();
    platform_net_cleanup();
    return 0;
}
