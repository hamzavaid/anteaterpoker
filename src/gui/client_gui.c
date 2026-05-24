#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "poker_gui.h"
#include "socket_client.h"

#define MAX_PLAYERS     6   // 1 local + up to 5 opponents
#define MAX_COMM_CARDS  5

//these define card pixel sizes at 3 scales for GUI rendering (small, medium, large)
#define CARD_W_SM       32
#define CARD_H_SM       46
#define CARD_W_MD       44
#define CARD_H_MD       63
#define CARD_W_LG       58
#define CARD_H_LG       83

static int g_server_fd = -1;

static GtkWidget* g_status_label = NULL;
static GtkWidget* g_pot_label = NULL;
static GtkWidget* g_stack_label = NULL;
static GtkWidget* g_raise_input = NULL;

static GtkWidget* g_comm_card_img[MAX_COMM_CARDS];
static GtkWidget* g_my_card_img[2];

//Opponent Slot struct to hold widgets for each opponent's display (name, bet, avatar, cards)
typedef struct {
    GtkWidget* name_label;
    GtkWidget* bet_label;
    GtkWidget* avatar_box;
    GtkWidget* card_img[2];
} OpponentSlot;

static OpponentSlot g_slots[MAX_PLAYERS - 1];

//CSS
static const char* POKER_CSS =

"#rankings_frame {"
"  background-color: rgba(14,22,34,0.96);"
"  border: 1px solid #1e4a6e;"
"  border-radius: 10px;"
"  padding: 6px;"
"}"
"#game_title {"
"  color: #7ab8e8;"
"  font-size: 20px;"
"  font-weight: bold;"
"  letter-spacing: 5px;"
"}"
"#game_subtitle {"
"  color: #1e4a6e;"
"  font-size: 8px;"
"  letter-spacing: 3px;"
"}"
"#slot_name {"
"  color: #5a9ac0;"
"  font-size: 9px;"
"  font-weight: bold;"
"  letter-spacing: 1px;"
"}"
"#slot_bet {"
"  color: #d4b830;"
"  font-size: 9px;"
"  font-weight: bold;"
"}"
"#slot_bet_folded {"
"  color: #4a5a6a;"
"  font-size: 9px;"
"}"
"#avatar_box {"
"  background-color: #0d1e2e;"
"  border: 2px solid #1e4a6e;"
"  border-radius: 21px;"
"}"
"#avatar_box_active {"
"  background-color: #0d1e2e;"
"  border: 2px solid #d4b830;"
"  border-radius: 21px;"
"}"
"#h_sep {"
"  background-color: rgba(30,74,110,0.5);"
"  min-height: 1px;"
"  margin: 2px 4px;"
"}"
"#comm_label {"
"  color: rgba(90,160,200,0.4);"
"  font-size: 8px;"
"  letter-spacing: 2px;"
"}"
"#pot_label_title {"
"  color: rgba(90,160,200,0.4);"
"  font-size: 8px;"
"  letter-spacing: 2px;"
"}"
"#pot_total {"
"  color: #d4b830;"
"  font-size: 12px;"
"  font-weight: bold;"
"}"
"#my_name {"
"  color: #7ab8e8;"
"  font-size: 11px;"
"  font-weight: bold;"
"  letter-spacing: 1px;"
"}"
"#my_stack {"
"  color: #d4b830;"
"  font-size: 10px;"
"}"
"#side_box {"
"  background-color: rgba(14,22,34,0.96);"
"  border: 1px solid #1e4a6e;"
"  border-radius: 10px;"
"  padding: 12px 14px;"
"}"
"#side_label {"
"  color: #2d6a9f;"
"  font-size: 8px;"
"  font-weight: bold;"
"  letter-spacing: 2px;"
"}"
"#status_text {"
"  color: #dce3ec;"
"  font-size: 12px;"
"  font-weight: bold;"
"}"
"#stack_text {"
"  color: #3d6a8a;"
"  font-size: 10px;"
"}"
"#raise_label {"
"  color: #3d6a8a;"
"  font-size: 9px;"
"}"
"#btn_call {"
"  background-color: #092a14;"
"  color: #42c46e;"
"  border: 1px solid #1a7040;"
"  border-radius: 6px;"
"  padding: 6px 10px;"
"  font-size: 12px;"
"  font-weight: bold;"
"  letter-spacing: 1px;"
"}"
"#btn_raise {"
"  background-color: #0f1e40;"
"  color: #7ab8e8;"
"  border: 1px solid #244a8a;"
"  border-radius: 6px;"
"  padding: 6px 10px;"
"  font-size: 12px;"
"  font-weight: bold;"
"  letter-spacing: 1px;"
"}"
"#btn_check {"
"  background-color: #241e08;"
"  color: #d4c040;"
"  border: 1px solid #6a5810;"
"  border-radius: 6px;"
"  padding: 6px 10px;"
"  font-size: 12px;"
"  font-weight: bold;"
"  letter-spacing: 1px;"
"}"
"#btn_fold {"
"  background-color: #2e0e0e;"
"  color: #e07070;"
"  border: 1px solid #7a1c1c;"
"  border-radius: 6px;"
"  padding: 6px 10px;"
"  font-size: 12px;"
"  font-weight: bold;"
"  letter-spacing: 1px;"
"}"
"#side_ctrl_btn {"
"  background-color: #0f2840;"
"  color: #7ab8e8;"
"  border: 1px solid #1e5080;"
"  border-radius: 7px;"
"  padding: 9px 12px;"
"  font-size: 11px;"
"  font-weight: bold;"
"  letter-spacing: 2px;"
"}"
"#side_ctrl_btn:hover {"
"  background-color: #153a5c;"
"  border-color: #3a8abf;"
"}";

//Update functions, call these from network/game code
void poker_gui_set_pot(int amount)
{
    if (!g_pot_label) return;
    char buf[64];
    snprintf(buf, sizeof buf, "%d", amount);
    gtk_label_set_text(GTK_LABEL(g_pot_label), buf);
}

void poker_gui_set_stack(int amount)
{
    if (!g_stack_label) return;
    char buf[64];
    snprintf(buf, sizeof buf, "Stack: %d", amount);
    gtk_label_set_text(GTK_LABEL(g_stack_label), buf);
}

void poker_gui_set_status(const char* msg)
{
    if (!g_status_label) return;
    gtk_label_set_text(GTK_LABEL(g_status_label), msg);
}

void poker_gui_set_community_card(int idx, const char* path)
{
    if (idx < 0 || idx >= MAX_COMM_CARDS) return;
    if (!g_comm_card_img[idx]) return;
    if (path) {
        GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(
            path, CARD_W_MD, CARD_H_MD, FALSE, NULL);
        gtk_image_set_from_pixbuf(GTK_IMAGE(g_comm_card_img[idx]), pb);
        if (pb) g_object_unref(pb);
    } else {
        gtk_image_clear(GTK_IMAGE(g_comm_card_img[idx]));
    }
}

void poker_gui_set_my_card(int idx, const char* path)
{
    if (idx < 0 || idx >= 2) return;
    if (!g_my_card_img[idx]) return;
    if (path) {
        GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(
            path, CARD_W_LG, CARD_H_LG, FALSE, NULL);
        gtk_image_set_from_pixbuf(GTK_IMAGE(g_my_card_img[idx]), pb);
        if (pb) g_object_unref(pb);
    } else {
        gtk_image_clear(GTK_IMAGE(g_my_card_img[idx]));
    }
}

//Update opponent slot display (name, bet, active, folded status)
void poker_gui_update_slot(int slot, const char* name, const char* bet_str, int is_active, int folded)
{
    if (slot < 0 || slot >= MAX_PLAYERS - 1) return;
    OpponentSlot* s = &g_slots[slot];

    if (s->name_label)
        gtk_label_set_text(GTK_LABEL(s->name_label), name ? name : "");

    if (s->bet_label) {
        gtk_label_set_text(GTK_LABEL(s->bet_label), bet_str ? bet_str : "");
        gtk_widget_set_name(s->bet_label,
            folded ? "slot_bet_folded" : "slot_bet");
    }

    if (s->avatar_box)
        gtk_widget_set_name(s->avatar_box,
            is_active ? "avatar_box_active" : "avatar_box");
}

static GtkWidget* make_card_back_sm(void)
{
    GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(
        "src/assets/back_of_card.png", CARD_W_SM, CARD_H_SM, FALSE, NULL);
    GtkWidget *img = gtk_image_new_from_pixbuf(pb);
    if (pb) g_object_unref(pb);
    gtk_widget_set_size_request(img, CARD_W_SM, CARD_H_SM);
    return img;
}

static GtkWidget* make_card_placeholder_md(void)
{
    GtkWidget* img = gtk_image_new();
    gtk_widget_set_size_request(img, CARD_W_MD, CARD_H_MD);
    return img;
}

static GtkWidget* make_card_placeholder_lg(void)
{
    GtkWidget* img = gtk_image_new();
    gtk_widget_set_size_request(img, CARD_W_LG, CARD_H_LG);
    return img;
}

/* builds a card image path from value and suit
   value: "ace","2"-"10","jack","queen","king"
   suit:  "spades","hearts","diamonds","clubs" */
void get_card_path(char* buf, int bufsize, const char* value, const char* suit)
{
    snprintf(buf, bufsize, "src/assets/%s_of_%s.png", value, suit);
}

static void on_call(GtkButton* b, gpointer d)
{
    (void)b; (void)d;
    poker_gui_set_status("You called.");
	send_to_server(g_server_fd, "ACTN:-1:CALL\n");
}

static void on_raise(GtkButton* b, gpointer d)
{
    (void)b; (void)d;
    if (!g_raise_input) return;
    const char* txt = gtk_entry_get_text(GTK_ENTRY(g_raise_input));
    char msg[64];
    snprintf(msg, sizeof msg, "RAISE:-1:%s\n", txt);
    poker_gui_set_status("You raised.");
    send_to_server(g_server_fd, msg);
}

static void on_check(GtkButton* b, gpointer d)
{
    (void)b; (void)d;
    poker_gui_set_status("You checked.");
    send_to_server(g_server_fd, "ACTN:-1:CHECK\n");
}

static void on_fold(GtkButton* b, gpointer d)
{
    (void)b; (void)d;
    poker_gui_set_status("You folded.");
    send_to_server(g_server_fd, "ACTN:-1:FOLD\n");
}

static void on_quit(GtkButton* b, gpointer d)
{
    (void)b;
    (void)d;

    if (g_server_fd >= 0)
    {
        send_to_server(g_server_fd, "QUIT:-1:bye\n");
        shutdown(g_server_fd, SHUT_RDWR);
        close(g_server_fd);
        g_server_fd = -1;
    }

    gtk_main_quit();
}

static GtkWidget* build_opponent_slot(int idx)
{
    OpponentSlot* s = &g_slots[idx];

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(vbox, GTK_ALIGN_CENTER);

    //two cards face down 
    GtkWidget* cards_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_widget_set_halign(cards_row, GTK_ALIGN_CENTER);
    s->card_img[0] = make_card_back_sm();
    s->card_img[1] = make_card_back_sm();
    gtk_box_pack_start(GTK_BOX(cards_row), s->card_img[0], FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cards_row), s->card_img[1], FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), cards_row, FALSE, FALSE, 0);

    //avatar frame 
    GtkWidget* avatar = gtk_frame_new(NULL);
    gtk_widget_set_name(avatar, "avatar_box");
    gtk_widget_set_size_request(avatar, 42, 42);
    gtk_widget_set_halign(avatar, GTK_ALIGN_CENTER);
    s->avatar_box = avatar;

    gtk_box_pack_start(GTK_BOX(vbox), avatar, FALSE, FALSE, 0);

    /* name and bet labels */
    char default_name[32];
    snprintf(default_name, sizeof default_name, "PLAYER %d", idx + 2);
    GtkWidget* name_lbl = gtk_label_new(default_name);
    gtk_widget_set_name(name_lbl, "slot_name");
    s->name_label = name_lbl;
    gtk_box_pack_start(GTK_BOX(vbox), name_lbl, FALSE, FALSE, 0);

    GtkWidget* bet_lbl = gtk_label_new("");
    gtk_widget_set_name(bet_lbl, "slot_bet");
    s->bet_label = bet_lbl;
    gtk_box_pack_start(GTK_BOX(vbox), bet_lbl, FALSE, FALSE, 0);

    return vbox;
}

static GtkWidget* build_right_panel(void)
{
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_size_request(vbox, 210, -1);

	//status box
    {
        GtkWidget* frame = gtk_frame_new(NULL);
        gtk_widget_set_name(frame, "side_box");
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_container_set_border_width(GTK_CONTAINER(inner), 4);
        gtk_container_add(GTK_CONTAINER(frame), inner);

        GtkWidget* lbl = gtk_label_new("STATUS");
        gtk_widget_set_name(lbl, "side_label");
        gtk_box_pack_start(GTK_BOX(inner), lbl, FALSE, FALSE, 0);

        g_status_label = gtk_label_new("Waiting...");
        gtk_widget_set_name(g_status_label, "status_text");
        gtk_label_set_line_wrap(GTK_LABEL(g_status_label), TRUE);
        gtk_widget_set_size_request(g_status_label, -1, 34);
        gtk_box_pack_start(GTK_BOX(inner), g_status_label, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    }

    //action box
    {
        GtkWidget* frame = gtk_frame_new(NULL);
        gtk_widget_set_name(frame, "side_box");
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_container_set_border_width(GTK_CONTAINER(inner), 4);
        gtk_container_add(GTK_CONTAINER(frame), inner);

        GtkWidget* lbl = gtk_label_new("YOUR ACTIONS");
        gtk_widget_set_name(lbl, "side_label");
        gtk_box_pack_start(GTK_BOX(inner), lbl, FALSE, FALSE, 0);

        g_stack_label = gtk_label_new("Stack: 0");
        gtk_widget_set_name(g_stack_label, "stack_text");
        gtk_box_pack_start(GTK_BOX(inner), g_stack_label, FALSE, FALSE, 0);

		//raise input row (label + entry)
        GtkWidget* raise_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        GtkWidget* raise_lbl = gtk_label_new("Raise to:");
        gtk_widget_set_name(raise_lbl, "raise_label");
        gtk_box_pack_start(GTK_BOX(raise_row), raise_lbl, FALSE, FALSE, 0);
        g_raise_input = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(g_raise_input), "400");
        gtk_widget_set_size_request(g_raise_input, 70, -1);
        gtk_box_pack_start(GTK_BOX(raise_row), g_raise_input, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(inner), raise_row, FALSE, FALSE, 0);

        //action buttons
        GtkWidget* btn_call = gtk_button_new_with_label("CALL");
        GtkWidget* btn_raise = gtk_button_new_with_label("RAISE");
        GtkWidget* btn_check = gtk_button_new_with_label("CHECK");
        GtkWidget* btn_fold = gtk_button_new_with_label("FOLD");
        gtk_widget_set_name(btn_call, "btn_call");
        gtk_widget_set_name(btn_raise, "btn_raise");
        gtk_widget_set_name(btn_check, "btn_check");
        gtk_widget_set_name(btn_fold, "btn_fold");
        gtk_box_pack_start(GTK_BOX(inner), btn_call, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(inner), btn_raise, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(inner), btn_check, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(inner), btn_fold, FALSE, FALSE, 0);
        g_signal_connect(btn_call, "clicked", G_CALLBACK(on_call), NULL);
        g_signal_connect(btn_raise, "clicked", G_CALLBACK(on_raise), NULL);
        g_signal_connect(btn_check, "clicked", G_CALLBACK(on_check), NULL);
        g_signal_connect(btn_fold, "clicked", G_CALLBACK(on_fold), NULL);

        gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    }

    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox), spacer, TRUE, TRUE, 0);

	//control box (quit button)
    {
        GtkWidget* frame = gtk_frame_new(NULL);
        gtk_widget_set_name(frame, "side_box");
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_container_set_border_width(GTK_CONTAINER(inner), 4);
        gtk_container_add(GTK_CONTAINER(frame), inner);

        GtkWidget* lbl = gtk_label_new("CONTROLS");
        gtk_widget_set_name(lbl, "side_label");
        gtk_box_pack_start(GTK_BOX(inner), lbl, FALSE, FALSE, 0);

        GtkWidget* btn_quit = gtk_button_new_with_label("QUIT");
        gtk_widget_set_name(btn_quit, "side_ctrl_btn");
        g_signal_connect(btn_quit, "clicked", G_CALLBACK(on_quit), NULL);
        gtk_box_pack_start(GTK_BOX(inner), btn_quit, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    }

    return vbox;
}

static GtkWidget* build_left_panel(void)
{
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(vbox, 280, -1);

    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox), spacer, TRUE, TRUE, 0);

    GtkWidget* frame = gtk_frame_new(NULL);
    gtk_widget_set_name(frame, "rankings_frame");
    GdkPixbuf *rpb = gdk_pixbuf_new_from_file_at_scale("src/assets/rankings.jpg", 268, -1, TRUE, NULL);
    GtkWidget* img = gtk_image_new_from_pixbuf(rpb);
    if (rpb) g_object_unref(rpb);
    gtk_widget_set_size_request(img, 268, -1);
 
    gtk_container_add(GTK_CONTAINER(frame), img);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
 
    return vbox;
}

static GtkWidget* build_center_panel(void)
{
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

	//title area
    GtkWidget* title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_halign(title_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_bottom(title_box, 4);
    GtkWidget* title = gtk_label_new("ANTEATER POKER");
    gtk_widget_set_name(title, "game_title");
    gtk_box_pack_start(GTK_BOX(title_box), title, FALSE, FALSE, 0);
    GtkWidget* sub = gtk_label_new("ZOTHOUSE");
    gtk_widget_set_name(sub, "game_subtitle");
    gtk_box_pack_start(GTK_BOX(title_box), sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), title_box, FALSE, FALSE, 0);

	//opponent area (up to 5 opponents in a row)
    GtkWidget* opp_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    for (int i = 0; i < MAX_PLAYERS - 1; i++) {
        GtkWidget* sp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start(GTK_BOX(opp_row), sp, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(opp_row), build_opponent_slot(i), FALSE, FALSE, 0);
    }
    GtkWidget* sp_end = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(opp_row), sp_end, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), opp_row, FALSE, FALSE, 0);

    GtkWidget* sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_name(sep1, "h_sep");
    gtk_box_pack_start(GTK_BOX(vbox), sep1, FALSE, FALSE, 2);

	//pot and community cards area
    GtkWidget* comm_area = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 28);
    gtk_widget_set_halign(comm_area, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(comm_area, 10);
    gtk_widget_set_margin_bottom(comm_area, 6);

    GtkWidget* pot_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_halign(pot_box, GTK_ALIGN_CENTER);
    GtkWidget* pot_title = gtk_label_new("POT");
    gtk_widget_set_name(pot_title, "pot_label_title");
    gtk_box_pack_start(GTK_BOX(pot_box), pot_title, FALSE, FALSE, 0);
    g_pot_label = gtk_label_new("0");
    gtk_widget_set_name(g_pot_label, "pot_total");
    gtk_box_pack_start(GTK_BOX(pot_box), g_pot_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(comm_area), pot_box, FALSE, FALSE, 0);

    GtkWidget* cards_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(cards_box, GTK_ALIGN_CENTER);
    GtkWidget* comm_lbl = gtk_label_new("COMMUNITY CARDS");
    gtk_widget_set_name(comm_lbl, "comm_label");
    gtk_box_pack_start(GTK_BOX(cards_box), comm_lbl, FALSE, FALSE, 0);
    GtkWidget* cards_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);
    gtk_widget_set_halign(cards_row, GTK_ALIGN_CENTER);
    for (int i = 0; i < MAX_COMM_CARDS; i++) {
        g_comm_card_img[i] = make_card_placeholder_md();
        gtk_box_pack_start(GTK_BOX(cards_row), g_comm_card_img[i], FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(cards_box), cards_row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(comm_area), cards_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), comm_area, FALSE, FALSE, 0);

    GtkWidget* sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_name(sep2, "h_sep");
    gtk_box_pack_start(GTK_BOX(vbox), sep2, FALSE, FALSE, 2);

    GtkWidget* my_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_halign(my_area, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(my_area, GTK_ALIGN_END);
    gtk_box_pack_end(GTK_BOX(vbox), my_area, FALSE, FALSE, 2);

    GtkWidget* my_hand_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_widget_set_halign(my_hand_row, GTK_ALIGN_CENTER);
    for (int i = 0; i < 2; i++) {
        g_my_card_img[i] = make_card_placeholder_lg();
        gtk_box_pack_start(GTK_BOX(my_hand_row), g_my_card_img[i], FALSE, FALSE, 0);
    }

    GtkWidget* my_info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_set_halign(my_info, GTK_ALIGN_CENTER);
    GtkWidget* my_av = gtk_frame_new(NULL);
    gtk_widget_set_name(my_av, "avatar_box");
    gtk_widget_set_size_request(my_av, 38, 38);
    gtk_widget_set_halign(my_av, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(my_info), my_av, FALSE, FALSE, 0);
    GtkWidget* my_name = gtk_label_new("YOU");
    gtk_widget_set_name(my_name, "my_name");
    gtk_box_pack_start(GTK_BOX(my_info), my_name, FALSE, FALSE, 0);
    g_stack_label = gtk_label_new("Stack: 0");
    gtk_widget_set_name(g_stack_label, "my_stack");
    gtk_box_pack_start(GTK_BOX(my_info), g_stack_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(my_hand_row), my_info, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(my_area), my_hand_row, FALSE, FALSE, 0);

    return vbox;
}

void launch_poker_window(int server_fd)
{
	g_server_fd = server_fd;
	
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, POKER_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(win, "poker_window");
    gtk_window_set_title(GTK_WINDOW(win), "Anteater Poker");
    gtk_window_set_default_size(GTK_WINDOW(win), 1100, 680);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    g_signal_connect(win, "destroy", G_CALLBACK(on_quit), NULL);

    GtkWidget *overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(win), overlay);

    GdkPixbuf *bg_pb = gdk_pixbuf_new_from_file_at_scale("src/assets/background.jpg", 1100, 680, FALSE, NULL);
    GtkWidget *bg = gtk_image_new_from_pixbuf(bg_pb);
    if (bg_pb) g_object_unref(bg_pb);
    gtk_widget_set_size_request(bg, 1100, 680);
    gtk_container_add(GTK_CONTAINER(overlay), bg);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(root), 10);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), root);
    gtk_widget_set_halign(root, GTK_ALIGN_FILL);
    gtk_widget_set_valign(root, GTK_ALIGN_FILL);

    gtk_box_pack_start(GTK_BOX(root), build_left_panel(),   FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root), build_center_panel(), TRUE,  TRUE,  0);
    gtk_box_pack_start(GTK_BOX(root), build_right_panel(),  FALSE, FALSE, 0);

    poker_gui_set_status("Waiting for players...");
    gtk_widget_show_all(win);
}
