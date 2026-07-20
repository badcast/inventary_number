#pragma once
#include <iostream>

#include <sigc++/sigc++.h>
#include <gtkmm.h>

#include "zint_barcoder_ritm.h"
#include "print_setup_window.h"
#include "add_data_window.h"
#include "add_person_window.h"
#include "db_manager.h"

class AppWindow : public Gtk::Window
{
public:
    AppWindow();
    ~AppWindow() override;

private:
    void setup_ui();
    void setup_actions();
    void load_table();
    void on_search_changed();
    void on_refresh();
    void update_connection_status(bool connected);
    void print_setup();
    void image_load_and_shown();
    void exportSqlDumpTo();

    void display_image(const Glib::RefPtr<Gdk::Pixbuf> &refPixBuf);

    Gtk::Box m_main_box {Gtk::Orientation::HORIZONTAL};
    Gtk::Box m_left_box {Gtk::Orientation::VERTICAL};
    Gtk::Box m_right_box {Gtk::Orientation::VERTICAL};

    Gtk::Frame m_gen_frame {};
    Gtk::Grid m_gen_grid;

    Gtk::Box m_db_btn_box {Gtk::Orientation::HORIZONTAL};
    Gtk::Button m_btn_add_data {"Добавить запись"};
    Gtk::Button m_btn_add_owner {"Добавить ответственного"};
    Gtk::Button m_btn_print {"Распечатать"};

    Gtk::Picture m_preview_picture;
    Gtk::Label m_preview_label;
    Gtk::Button m_btn_load_img {"Открыть изо"};

    Gtk::Box m_search_box {Gtk::Orientation::HORIZONTAL};
    Gtk::Entry m_search_entry;
    Gtk::Button m_btn_refresh {"Искать"};

    Gtk::ScrolledWindow m_scrolled;
    Gtk::TreeView m_tree_view;
    Glib::RefPtr<Gtk::ListStore> m_list_model;

    class ModelColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:
        ModelColumns()
        {
            add(col_id);
            add(col_image);
            add(col_code);
            add(col_name);
            add(col_desc);
            add(col_owner);
            add(col_loc);
            add(col_cdate);
        }
        Gtk::TreeModelColumn<int> col_id,col_image;
        Gtk::TreeModelColumn<Glib::ustring> col_code, col_name, col_desc, col_owner, col_cdate, col_loc;
    };
    ModelColumns m_data_columns;

    Gtk::Label m_status_label;
};
