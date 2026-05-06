#include "add_person_window.h"

AddOwnerWindow::AddOwnerWindow(Gtk::Window &parent) : Gtk::Dialog("Добавление ответственного лица", parent, true), m_content_box(Gtk::Orientation::VERTICAL), m_label_name("ФИО ответственного:"), m_label_room("Номер кабинета (оставьте пустым если нет кабинета):")
{
    set_modal(true);
    set_resizable(false);
    set_default_size(500, -1);
    set_default_response(Gtk::ResponseType::CANCEL);

    auto content_area = get_content_area();
    content_area->set_margin_top(12);
    content_area->set_margin_bottom(12);
    content_area->set_margin_start(12);
    content_area->set_margin_end(12);

    m_content_box.set_spacing(8);

    m_content_box.append(m_label_name);
    m_content_box.append(m_entry_name);
    m_content_box.append(m_label_room);
    m_content_box.append(m_entry_room);

    content_area->append(m_content_box);

    m_button_box.set_spacing(8);
    m_button_box.set_halign(Gtk::Align::END);
    m_button_box.set_margin_top(12);

    m_button_box.append(m_btn_cancel);
    m_button_box.append(m_btn_save);

    content_area->append(m_button_box);

    m_btn_save.signal_clicked().connect(sigc::mem_fun(*this, &AddOwnerWindow::on_save_clicked));

    m_btn_cancel.signal_clicked().connect(sigc::mem_fun(*this, &AddOwnerWindow::close));
}

AddOwnerWindow::~AddOwnerWindow()
{
}

AddOwnerWindow *AddOwnerWindow::show_as(Gtk::Window &parent)
{
    auto *dialog = new AddOwnerWindow(parent);
    dialog->set_modal(true);

    dialog->signal_response().connect(
        [dialog](int response_id)
        {
            dialog->hide();
            delete dialog;
        });

    dialog->show();
    return dialog;
}

std::string AddOwnerWindow::get_room() const
{
    return m_entry_room.get_text();
}

void AddOwnerWindow::show_error(const std::string &message)
{
    auto dialog = Gtk::AlertDialog::create();
    dialog->set_message(message);
    dialog->show(*this);
}

void AddOwnerWindow::on_save_clicked()
{
    std::string name = m_entry_name.get_text();
    std::string cabinet = m_entry_room.get_text();

    if(name.empty())
    {
        show_error("ФИО не может быть пустым.");
        return;
    }

    if(!DbManager::get().insert_owner(name, cabinet))
    {
        show_error("Ошибка. Ошибка базы данных.");
        return;
    }

    response(Gtk::ResponseType::OK);
}
