#pragma once
#include <gtkmm.h>
#include "db_manager.h"

class AddDataWindow : public Gtk::Window
{
public:
    AddDataWindow(Gtk::Window &parent);
    virtual ~AddDataWindow() override;

    static void show_as(Gtk::Window &parent);

protected:
    void on_save_clicked();

    Gtk::Box m_main_vbox {Gtk::Orientation::VERTICAL};
    Gtk::Box m_button_box {Gtk::Orientation::HORIZONTAL};

    Gtk::Label m_label_type;
    Gtk::ComboBoxText m_combo_types;
    Gtk::Label m_label_name;
    Gtk::Entry m_entry_name;
    Gtk::Label m_label_owner;
    Gtk::ComboBoxText m_combo_owners;

    Gtk::Button m_btn_save {"Сохранить"};
    Gtk::Button m_btn_cancel {"Отменить"};

    void show_error(const std::string &message);
};