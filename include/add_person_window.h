#pragma once
#include <string>
#include <utility>
#include <gtkmm.h>
#include "db_manager.h"

class AddOwnerWindow : public Gtk::Dialog
{
public:
    AddOwnerWindow(Gtk::Window &parent);
    virtual ~AddOwnerWindow();

    static AddOwnerWindow *show_as(Gtk::Window &parent);

    std::string get_name() const;
    std::string get_room() const;

protected:
    Gtk::Box m_content_box;
    Gtk::Box m_button_box;

    Gtk::Entry m_entry_name;
    Gtk::Entry m_entry_room;
    Gtk::Label m_label_name;
    Gtk::Label m_label_room;

    Gtk::Button m_btn_save {"Добавить"};
    Gtk::Button m_btn_cancel {"Отменить"};

    void on_save_clicked();
    void show_error(const std::string &message);
};
