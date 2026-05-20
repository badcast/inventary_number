#pragma once

#include <cmath>
#include <iostream>
#include <gtkmm.h>
#include <vector>

#include "add_data_window.h"
#include "zint_barcoder_ritm.h"

enum class CutLineType
{
    NONE,
    HLINE,
    VLINE,
    BOTH
};

class PrintSetupWindow : public Gtk::Window
{
public:
    PrintSetupWindow(Gtk::Window &parent);
    ~PrintSetupWindow() override = default;

    static PrintSetupWindow *show_as(Gtk::Window &parent, std::vector<MemberFieldData> data);

protected:
    void init_ui();
    void init_print();
    void init_actions();

    void create_menubar();
    void create_toolbar();
    void create_statusbar();

    void on_print();
    void on_page_setup();

    void on_prev_page();
    void on_next_page();
    void draw_preview_page();
    void update_status();
    void soft_update_state();
    void draw_print_page(const Glib::RefPtr<Gtk::PrintContext> &context, int page_nr);
    void generate_page_content(Cairo::RefPtr<Cairo::Context> cr, int page_nr, double width_pt, double height_pt, bool quality);

    Gtk::Box m_box {Gtk::Orientation::VERTICAL};
    Gtk::Paned m_paned {Gtk::Orientation::HORIZONTAL};
    Gtk::Box m_left_box {Gtk::Orientation::VERTICAL, 6};
    Gtk::Box m_right_box {Gtk::Orientation::VERTICAL, 6};
    Gtk::Box m_controls {Gtk::Orientation::VERTICAL, 12};
    Gtk::Box m_nav {Gtk::Orientation::HORIZONTAL, 6};
    Gtk::Box m_bottom_box {Gtk::Orientation::HORIZONTAL};

    Gtk::PopoverMenuBar m_menubar;
    Gtk::Box m_toolbar {Gtk::Orientation::HORIZONTAL};
    Gtk::Statusbar m_statusbar;

    Gtk::Picture m_preview_page;
    Gtk::Frame m_preview_frame;
    Gtk::ScrolledWindow m_scroll_preview;
    Gtk::DrawingArea m_bg_area;

    Gtk::ComboBoxText m_combo_cutline;
    Gtk::CheckButton m_chb_no_border {"Без полей"};
    Gtk::CheckButton m_chb_show_text {"Показывать текст"};

    Gtk::Button m_btn_prev;
    Gtk::Button m_btn_next;
    Gtk::Label m_lbl_page;
    Gtk::Button m_btn_print {"Печать"};

    Glib::RefPtr<Gtk::PageSetup> m_page_setup;
    Glib::RefPtr<Gtk::PrintSettings> m_print_settings;
    Glib::RefPtr<Gio::SimpleActionGroup> m_actions;

    int m_page = 0;
    int m_pages_total = 1;
    CutLineType m_cutline = CutLineType::NONE;
    bool m_updating_preset = false;
    std::vector<MemberFieldData> m_data;

    double m_item_width_cm = 7;
    double m_item_height_cm = 1;
    static constexpr double CM_TO_PT = 28.346;
};