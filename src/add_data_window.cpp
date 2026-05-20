#include "add_data_window.h"

AddDataWindow::AddDataWindow(Gtk::Window &parent) : m_label_type("Тип инвентаря:"), m_label_owner("Ответственен:"), m_label_name("Ярлык:")
{
    set_title("Добавление новой записи");
    set_transient_for(parent);
    set_modal(true);
    set_default_size(400, -1);
    set_resizable(false);

    m_main_vbox.set_margin(15);
    m_main_vbox.set_spacing(10);
    set_child(m_main_vbox);

    m_label_type.set_halign(Gtk::Align::START);
    m_label_owner.set_halign(Gtk::Align::START);
    m_label_name.set_halign(Gtk::Align::START);

    for(const auto &_class : DbManager::get().get_types())
        m_combo_types.append(_class->short_code + " - " + _class->name);
    m_combo_types.set_active(0);

    m_combo_owners.append("(Не назначено)");
    for(const auto &_owner : DbManager::get().get_owners())
        m_combo_owners.append(_owner.name + " - " + _owner.room);
    m_combo_owners.set_active(0);

    m_main_vbox.append(m_label_name);
    m_main_vbox.append(m_entry_name);
    m_main_vbox.append(m_label_type);
    m_main_vbox.append(m_combo_types);
    m_main_vbox.append(m_label_owner);
    m_main_vbox.append(m_combo_owners);

    m_button_box.set_spacing(10);
    m_button_box.set_halign(Gtk::Align::END);
    m_button_box.set_margin_top(10);

    m_button_box.append(m_btn_cancel);
    m_button_box.append(m_btn_save);
    m_main_vbox.append(m_button_box);

    m_btn_save.signal_clicked().connect(sigc::mem_fun(*this, &AddDataWindow::on_save_clicked));
    m_btn_cancel.signal_clicked().connect(sigc::mem_fun(*this, &AddDataWindow::close));
}

AddDataWindow::~AddDataWindow()
{
}

void AddDataWindow::show_as(Gtk::Window &parent)
{
    auto *window = new AddDataWindow(parent);

    window->signal_hide().connect(
        [window]()
        {
            Glib::signal_idle().connect(
                [window]()
                {
                    delete window;
                    return false;
                });
        });

    window->present();
}

void AddDataWindow::on_save_clicked()
{
    int target_type_id = m_combo_types.get_active_row_number();
    int target_owner_id = m_combo_owners.get_active_row_number() - 1;
    int insert_id;

    if(m_entry_name.get_text().length() < 3)
    {
        show_error("Ошибка. Поля ярлык не может быть пустым и не менее 3 символов.");
        return;
    }

    if(target_type_id == -1)
    {
        show_error("Ошибка. Выберите данные из списка и попробуйте снова.");
        return;
    }

    if(target_owner_id > 0)
    {
        target_owner_id = DbManager::get().get_owners()[target_owner_id].id;
    }
    else
    {
        target_owner_id = std::clamp(target_owner_id, -1, std::numeric_limits<int>::max());
    }

    if(!DbManager::get().insert_data(DbManager::get().get_types()[target_type_id]->short_code, m_entry_name.get_text(), {}, target_owner_id, &insert_id))
    {
        show_error("Ошибка. Ошибка базы данных.");
        return;
    }

    close();
}

void AddDataWindow::show_error(const std::string &message)
{
    auto alert = Gtk::AlertDialog::create(message);
    alert->show(*this);
}