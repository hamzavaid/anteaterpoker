#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 
#include "game_state.h"
#include "server_gui.h"
 
#define MAX_PLAYERS 6
#define MAX_COMM_CARDS 5

static GameState *g_game = NULL;
static GtkWidget *g_phase_label     = NULL;
static GtkWidget *g_pot_label       = NULL;
static GtkWidget *g_turn_label      = NULL;
static GtkWidget *g_player_count    = NULL;
static GtkWidget *g_comm_card_img[MAX_COMM_CARDS];

// one row of widgets per player seat in the player table
typedef struct {
    GtkWidget *seat_label;
    GtkWidget *name_label;
    GtkWidget *status_label;
    GtkWidget *points_label;
    GtkWidget *bet_label;
    GtkWidget *ability_label;
} PlayerRow;
 
static PlayerRow g_rows[MAX_PLAYERS];

//CSS
static const char *SERVER_CSS =
 
    "#server_title {"
    "  color: #7ab8e8;"
    "  font-size: 18px;"
    "  font-weight: bold;"
    "  letter-spacing: 4px;"
    "}"
    "#server_subtitle {"
    "  color: #1e4a6e;"
    "  font-size: 8px;"
    "  letter-spacing: 3px;"
    "}"
    "#side_box {"
    "  background-color: rgba(14,22,34,0.96);"
    "  border: 1px solid #1e4a6e;"
    "  border-radius: 10px;"
    "  padding: 10px 12px;"
    "}"
    "#side_label {"
    "  color: #2d6a9f;"
    "  font-size: 8px;"
    "  font-weight: bold;"
    "  letter-spacing: 2px;"
    "}"
    "#stat_value {"
    "  color: #dce3ec;"
    "  font-size: 13px;"
    "  font-weight: bold;"
    "}"
    "#stat_gold {"
    "  color: #d4b830;"
    "  font-size: 13px;"
    "  font-weight: bold;"
    "}"
    "#phase_label {"
    "  color: #7ab8e8;"
    "  font-size: 22px;"
    "  font-weight: bold;"
    "  letter-spacing: 3px;"
    "}"
    "#table_header {"
    "  color: #2d6a9f;"
    "  font-size: 8px;"
    "  font-weight: bold;"
    "  letter-spacing: 1px;"
    "}"
    "#table_cell {"
    "  color: #7aadcc;"
    "  font-size: 10px;"
    "}"
    "#status_active {"
    "  color: #42c46e;"
    "  font-size: 10px;"
    "  font-weight: bold;"
    "}"
    "#status_connected {"
    "  color: #7ab8e8;"
    "  font-size: 10px;"
    "}"
    "#status_folded {"
    "  color: #e07070;"
    "  font-size: 10px;"
    "}"
    "#status_empty {"
    "  color: #2d4a5a;"
    "  font-size: 10px;"
    "}"
    "#ctrl_btn_green {"
    "  background-color: #092a14;"
    "  color: #42c46e;"
    "  border: 1px solid #1a7040;"
    "  border-radius: 6px;"
    "  padding: 8px 12px;"
    "  font-size: 11px;"
    "  font-weight: bold;"
    "  letter-spacing: 1px;"
    "}"
    "#ctrl_btn_blue {"
    "  background-color: #0f1e40;"
    "  color: #7ab8e8;"
    "  border: 1px solid #244a8a;"
    "  border-radius: 6px;"
    "  padding: 8px 12px;"
    "  font-size: 11px;"
    "  font-weight: bold;"
    "  letter-spacing: 1px;"
    "}"
    "#ctrl_btn_red {"
    "  background-color: #2e0e0e;"
    "  color: #e07070;"
    "  border: 1px solid #7a1c1c;"
    "  border-radius: 6px;"
    "  padding: 8px 12px;"
    "  font-size: 11px;"
    "  font-weight: bold;"
    "  letter-spacing: 1px;"
    "}"
    "#comm_label {"
    "  color: rgba(90,160,200,0.4);"
    "  font-size: 8px;"
    "  letter-spacing: 2px;"
    "}"
    "#h_sep {"
    "  background-color: rgba(30,74,110,0.5);"
    "  min-height: 1px;"
    "  margin: 2px 4px;"
    "}";

    //returns CSS widget name for status colors
    static const char *status_css_name(PlayerStatus status)
{
    switch (status) {
        case PLAYER_ACTIVE:    return "status_active";
        case PLAYER_CONNECTED: return "status_connected";
        case PLAYER_FOLDED:    return "status_folded";
        default:               return "status_empty";
    }
}

//returns string for status
static const char *status_string(PlayerStatus status)
{
    switch (status) {
        case PLAYER_ACTIVE:    return "ACTIVE";
        case PLAYER_CONNECTED: return "CONNECTED";
        case PLAYER_FOLDED:    return "FOLDED";
        default:               return "---";
    }
}

static void card_to_asset_path(Card card, char *buf, int bufsize)
{
    char tmp[64];
    card_to_string(card, tmp, sizeof tmp);
    // card_to_string should produce something like "ace_of_spades"
    snprintf(buf, bufsize, "src/assets/%s.png", tmp);
}

//reads g_game and updates every widget on screen.
void server_gui_refresh(void)
{
    if (!g_game) return;
 
    // phase
    if (g_phase_label)
        gtk_label_set_text(GTK_LABEL(g_phase_label),
                           game_phase_to_string(g_game->phase));
 
    // pot
    if (g_pot_label) {
        char buf[64];
        snprintf(buf, sizeof buf, "%d", g_game->pot);
        gtk_label_set_text(GTK_LABEL(g_pot_label), buf);
    }
 
    // current turn
    if (g_turn_label) {
        char buf[64];
        if (g_game->current_turn >= 0 &&
            g_game->players[g_game->current_turn].status != PLAYER_EMPTY) {
            snprintf(buf, sizeof buf, "Seat %d — %s",
                     g_game->current_turn,
                     g_game->players[g_game->current_turn].name);
        } else {
            snprintf(buf, sizeof buf, "—");
        }
        gtk_label_set_text(GTK_LABEL(g_turn_label), buf);
    }
 
    // player count
    if (g_player_count) {
        char buf[32];
        snprintf(buf, sizeof buf, "%d / %d", g_game->player_count, MAX_PLAYERS);
        gtk_label_set_text(GTK_LABEL(g_player_count), buf);
    }
 
    // community cards
    for (int i = 0; i < MAX_COMM_CARDS; i++) {
        if (!g_comm_card_img[i]) continue;
        if (i < g_game->community_count) {
            char path[128];
            card_to_asset_path(g_game->community_cards[i], path, sizeof path);
            GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(
                path, 44, 63, FALSE, NULL);
            gtk_image_set_from_pixbuf(GTK_IMAGE(g_comm_card_img[i]), pb);
            if (pb) g_object_unref(pb);
        } else {
            gtk_image_clear(GTK_IMAGE(g_comm_card_img[i]));
        }
    }
    // player rows
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player *p = &g_game->players[i];
        PlayerRow *r = &g_rows[i];
 
        if (!r->name_label) continue;
 
        if (p->status == PLAYER_EMPTY) {
            gtk_label_set_text(GTK_LABEL(r->name_label),    "---");
            gtk_label_set_text(GTK_LABEL(r->status_label),  "---");
            gtk_label_set_text(GTK_LABEL(r->points_label),  "---");
            gtk_label_set_text(GTK_LABEL(r->bet_label),     "---");
            gtk_label_set_text(GTK_LABEL(r->ability_label), "---");
        } else {
            gtk_label_set_text(GTK_LABEL(r->name_label), p->name);
 
            gtk_label_set_text(GTK_LABEL(r->status_label), status_string(p->status));
            gtk_widget_set_name(r->status_label, status_css_name(p->status));
 
            char points_str[32], bet_str[32];
            snprintf(points_str, sizeof points_str, "%d", p->points);
            snprintf(bet_str,    sizeof bet_str,    "%d", p->current_bet);
            gtk_label_set_text(GTK_LABEL(r->points_label), points_str);
            gtk_label_set_text(GTK_LABEL(r->bet_label),    bet_str);
 
            gtk_label_set_text(GTK_LABEL(r->ability_label),
                               ability_to_string(p->ability.type));
 
            }
    }
}

static void on_start_hand(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    if (!g_game) return;
    start_new_hand(g_game);
    server_gui_refresh();
}
 
static void on_deal_flop(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    if (!g_game) return;
    deal_flop(g_game);
    server_gui_refresh();
}
 
static void on_deal_turn(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    if (!g_game) return;
    deal_turn(g_game);
    server_gui_refresh();
}
 
static void on_deal_river(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    if (!g_game) return;
    deal_river(g_game);
    server_gui_refresh();
}
 
static void on_quit(GtkButton *b, gpointer d)
{
    (void)b; (void)d;
    gtk_main_quit();
}

static GtkWidget *build_player_table(void)
{
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
 
    // header row
    const char *headers[] = { "SEAT", "NAME", "STATUS", "POINTS", "BET", "ABILITY" };
    for (int c = 0; c < 6; c++) {
        GtkWidget *h = gtk_label_new(headers[c]);
        gtk_widget_set_name(h, "table_header");
        gtk_widget_set_halign(h, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), h, c, 0, 1, 1);
    }
 
    // one row per seat
    for (int i = 0; i < MAX_PLAYERS; i++) {
        PlayerRow *r = &g_rows[i];
        int row = i + 1;
 
        char seat_str[8];
        snprintf(seat_str, sizeof seat_str, "%d", i);
 
        r->seat_label    = gtk_label_new(seat_str);
        r->name_label    = gtk_label_new("---");
        r->status_label  = gtk_label_new("---");
        r->points_label  = gtk_label_new("---");
        r->bet_label     = gtk_label_new("---");
        r->ability_label = gtk_label_new("---");
 
        GtkWidget *cells[] = {
            r->seat_label, r->name_label, r->status_label,
            r->points_label, r->bet_label, r->ability_label
        };
 
        for (int c = 0; c < 6; c++) {
            gtk_widget_set_name(cells[c], "table_cell");
            gtk_widget_set_halign(cells[c], GTK_ALIGN_START);
            gtk_grid_attach(GTK_GRID(grid), cells[c], c, row, 1, 1);
        }
    }
 
    return grid;
}

static GtkWidget *build_left_panel(void)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_size_request(vbox, 260, -1);
 
    // game stats box
    {
        GtkWidget *frame = gtk_frame_new(NULL);
        gtk_widget_set_name(frame, "side_box");
        GtkWidget *inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_set_border_width(GTK_CONTAINER(inner), 4);
        gtk_container_add(GTK_CONTAINER(frame), inner);
 
        GtkWidget *lbl = gtk_label_new("GAME STATE");
        gtk_widget_set_name(lbl, "side_label");
        gtk_box_pack_start(GTK_BOX(inner), lbl, FALSE, FALSE, 0);
 
        // phase
        g_phase_label = gtk_label_new("LOBBY");
        gtk_widget_set_name(g_phase_label, "phase_label");
        gtk_widget_set_halign(g_phase_label, GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(inner), g_phase_label, FALSE, FALSE, 4);
 
        GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_set_name(sep, "h_sep");
        gtk_box_pack_start(GTK_BOX(inner), sep, FALSE, FALSE, 2);
 
        // pot row
        GtkWidget *pot_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *pot_lbl = gtk_label_new("POT");
        gtk_widget_set_name(pot_lbl, "side_label");
        gtk_box_pack_start(GTK_BOX(pot_row), pot_lbl, FALSE, FALSE, 0);
        g_pot_label = gtk_label_new("0");
        gtk_widget_set_name(g_pot_label, "stat_gold");
        gtk_box_pack_start(GTK_BOX(pot_row), g_pot_label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(inner), pot_row, FALSE, FALSE, 0);
 
        // players row
        GtkWidget *players_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *players_lbl = gtk_label_new("PLAYERS");
        gtk_widget_set_name(players_lbl, "side_label");
        gtk_box_pack_start(GTK_BOX(players_row), players_lbl, FALSE, FALSE, 0);
        g_player_count = gtk_label_new("0 / 6");
        gtk_widget_set_name(g_player_count, "stat_value");
        gtk_box_pack_start(GTK_BOX(players_row), g_player_count, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(inner), players_row, FALSE, FALSE, 0);
 
        // current turn row
        GtkWidget *turn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *turn_lbl_hdr = gtk_label_new("TURN");
        gtk_widget_set_name(turn_lbl_hdr, "side_label");
        gtk_box_pack_start(GTK_BOX(turn_row), turn_lbl_hdr, FALSE, FALSE, 0);
        g_turn_label = gtk_label_new("—");
        gtk_widget_set_name(g_turn_label, "stat_value");
        gtk_label_set_line_wrap(GTK_LABEL(g_turn_label), TRUE);
        gtk_box_pack_start(GTK_BOX(turn_row), g_turn_label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(inner), turn_row, FALSE, FALSE, 0);
 
        gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    }
 
    // community cards box
    {
        GtkWidget *frame = gtk_frame_new(NULL);
        gtk_widget_set_name(frame, "side_box");
        GtkWidget *inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_set_border_width(GTK_CONTAINER(inner), 4);
        gtk_container_add(GTK_CONTAINER(frame), inner);
 
        GtkWidget *lbl = gtk_label_new("COMMUNITY CARDS");
        gtk_widget_set_name(lbl, "side_label");
        gtk_box_pack_start(GTK_BOX(inner), lbl, FALSE, FALSE, 0);
 
        GtkWidget *cards_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_widget_set_halign(cards_row, GTK_ALIGN_CENTER);
        for (int i = 0; i < MAX_COMM_CARDS; i++) {
            g_comm_card_img[i] = gtk_image_new();
            gtk_widget_set_size_request(g_comm_card_img[i], 44, 63);
            gtk_box_pack_start(GTK_BOX(cards_row), g_comm_card_img[i],
                               FALSE, FALSE, 0);
        }
        gtk_box_pack_start(GTK_BOX(inner), cards_row, FALSE, FALSE, 0);
 
        gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    }
 
    // controls box
    {
        GtkWidget *frame = gtk_frame_new(NULL);
        gtk_widget_set_name(frame, "side_box");
        GtkWidget *inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_set_border_width(GTK_CONTAINER(inner), 4);
        gtk_container_add(GTK_CONTAINER(frame), inner);
 
        GtkWidget *lbl = gtk_label_new("GAME CONTROLS");
        gtk_widget_set_name(lbl, "side_label");
        gtk_box_pack_start(GTK_BOX(inner), lbl, FALSE, FALSE, 0);
 
        // Start Hand button — calls start_new_hand()
        GtkWidget *btn_start = gtk_button_new_with_label("START NEW HAND");
        gtk_widget_set_name(btn_start, "ctrl_btn_green");
        g_signal_connect(btn_start, "clicked", G_CALLBACK(on_start_hand), NULL);
        gtk_box_pack_start(GTK_BOX(inner), btn_start, FALSE, FALSE, 0);
 
        // Deal Flop button — calls deal_flop()
        GtkWidget *btn_flop = gtk_button_new_with_label("DEAL FLOP");
        gtk_widget_set_name(btn_flop, "ctrl_btn_blue");
        g_signal_connect(btn_flop, "clicked", G_CALLBACK(on_deal_flop), NULL);
        gtk_box_pack_start(GTK_BOX(inner), btn_flop, FALSE, FALSE, 0);
 
        // Deal Turn button — calls deal_turn()
        GtkWidget *btn_turn = gtk_button_new_with_label("DEAL TURN");
        gtk_widget_set_name(btn_turn, "ctrl_btn_blue");
        g_signal_connect(btn_turn, "clicked", G_CALLBACK(on_deal_turn), NULL);
        gtk_box_pack_start(GTK_BOX(inner), btn_turn, FALSE, FALSE, 0);
 
        // Deal River button — calls deal_river()
        GtkWidget *btn_river = gtk_button_new_with_label("DEAL RIVER");
        gtk_widget_set_name(btn_river, "ctrl_btn_blue");
        g_signal_connect(btn_river, "clicked", G_CALLBACK(on_deal_river), NULL);
        gtk_box_pack_start(GTK_BOX(inner), btn_river, FALSE, FALSE, 0);
 
        GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_set_name(sep, "h_sep");
        gtk_box_pack_start(GTK_BOX(inner), sep, FALSE, FALSE, 2);
 
        GtkWidget *btn_quit = gtk_button_new_with_label("QUIT SERVER");
        gtk_widget_set_name(btn_quit, "ctrl_btn_red");
        g_signal_connect(btn_quit, "clicked", G_CALLBACK(on_quit), NULL);
        gtk_box_pack_start(GTK_BOX(inner), btn_quit, FALSE, FALSE, 0);
 
        gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    }
 
    return vbox;
}

static GtkWidget *build_right_panel(void)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
 
    // player table box
    {
        GtkWidget *frame = gtk_frame_new(NULL);
        gtk_widget_set_name(frame, "side_box");
        GtkWidget *inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_set_border_width(GTK_CONTAINER(inner), 6);
        gtk_container_add(GTK_CONTAINER(frame), inner);
 
        GtkWidget *lbl = gtk_label_new("PLAYERS AT TABLE");
        gtk_widget_set_name(lbl, "side_label");
        gtk_box_pack_start(GTK_BOX(inner), lbl, FALSE, FALSE, 0);
 
        gtk_box_pack_start(GTK_BOX(inner), build_player_table(), FALSE, FALSE, 0);
 
        gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    }
 
    return vbox;
}

void launch_server_window(GameState *game)
{
    g_game = game;
 
    // load CSS
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, SERVER_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
 
    // create window
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(win, "server_window");
    gtk_window_set_title(GTK_WINDOW(win), "Anteater Poker — Server");
    gtk_window_set_default_size(GTK_WINDOW(win), 1100, 700);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    g_signal_connect(win, "destroy", G_CALLBACK(on_quit), NULL);
 
    GtkWidget *overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(win), overlay);
 
     GdkPixbuf *bg_pb = gdk_pixbuf_new_from_file_at_scale(
        "src/assets/background.jpg", 1100, 700, FALSE, NULL);
    GtkWidget *bg = gtk_image_new_from_pixbuf(bg_pb);
    if (bg_pb) g_object_unref(bg_pb);
    gtk_widget_set_size_request(bg, 1100, 700);
    gtk_container_add(GTK_CONTAINER(overlay), bg);
 
    GtkWidget *root_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(root_vbox), 12);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), root_vbox);
    gtk_widget_set_halign(root_vbox, GTK_ALIGN_FILL);
    gtk_widget_set_valign(root_vbox, GTK_ALIGN_FILL);
 
    // title bar
    GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_halign(title_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_bottom(title_box, 4);
    GtkWidget *title = gtk_label_new("ANTEATER POKER — CONTROL ROOM");
    gtk_widget_set_name(title, "server_title");
    gtk_box_pack_start(GTK_BOX(title_box), title, FALSE, FALSE, 0);
    GtkWidget *sub = gtk_label_new(game->config.table_name);
    gtk_widget_set_name(sub, "server_subtitle");
    gtk_box_pack_start(GTK_BOX(title_box), sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root_vbox), title_box, FALSE, FALSE, 0);
 
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_name(sep, "h_sep");
    gtk_box_pack_start(GTK_BOX(root_vbox), sep, FALSE, FALSE, 0);
 
    // two-column layout: [left controls] [right player table + log]
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_box_pack_start(GTK_BOX(root_vbox), hbox, TRUE, TRUE, 0);
 
    gtk_box_pack_start(GTK_BOX(hbox), build_left_panel(),  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), build_right_panel(), TRUE,  TRUE,  0);
 
    server_gui_refresh();
 
    gtk_widget_show_all(win);
}
