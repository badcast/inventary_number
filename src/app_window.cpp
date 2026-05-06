#include "app_window.h"

AppWindow::AppWindow()
{
    set_title("RITM Medical Center | Инвентарный Менеджер");
    set_default_size(1200, 800);
    setup_ui();
    setup_actions();

    DbManager::get().signal_connection_changed.connect(sigc::mem_fun(*this, &AppWindow::update_connection_status));
    auto &db = DbManager::get();
    db.connect_db("localhost", 3306, "barcode_db", "invent", "123");
    load_table();
}

AppWindow::~AppWindow()
{
}

void AppWindow::setup_ui()
{
    // Init CSS Provider
    // auto css_provider = Gtk::CssProvider::create();
    // css_provider->load_from_string("entry:placeholder {"
    //                                "  font-style: italic;"
    //                                "  color: #888888;"
    //                                "  opacity: 0.8;"
    //                                "}");
    // auto display = Gdk::Display::get_default();
    // Gtk::StyleContext::add_provider_for_display(display, css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    set_child(m_main_box);
    m_main_box.append(m_left_box);
    m_main_box.append(m_right_box);
    m_left_box.set_margin(10);
    m_right_box.set_margin(10);
    m_left_box.set_size_request(100, -1);

    m_left_box.append(m_gen_frame);
    m_gen_frame.set_child(m_gen_grid);
    m_gen_grid.set_margin(10);
    m_gen_grid.set_row_spacing(8);
    m_gen_grid.set_column_spacing(8);

    m_left_box.append(m_db_btn_box);
    m_db_btn_box.set_spacing(5);
    m_db_btn_box.append(m_btn_add_data);
    m_db_btn_box.append(m_btn_add_owner);
    m_db_btn_box.append(m_btn_print);

    m_preview_picture.set_halign(Gtk::Align::CENTER);
    m_preview_picture.set_valign(Gtk::Align::CENTER);
    m_preview_picture.set_hexpand(false);
    m_preview_picture.set_vexpand(false);
    m_preview_picture.set_size_request(100, 100);
    m_left_box.append(m_preview_picture);

    m_right_box.append(m_search_box);
    m_search_entry.set_hexpand(true);
    m_search_entry.set_placeholder_text("Введите запрос для поиска: Например, Иван");
    m_search_box.append(m_search_entry);
    m_search_box.append(m_btn_refresh);

    m_right_box.append(m_scrolled);
    m_scrolled.set_child(m_tree_view);
    m_scrolled.set_expand(true);

    m_tree_view.get_selection()->set_mode(Gtk::SelectionMode::MULTIPLE);
    m_list_model = Gtk::ListStore::create(m_data_columns);
    m_tree_view.set_model(m_list_model);
    // m_tree_view.append_column("ID", m_data_columns.col_id);
    m_tree_view.append_column("Код", m_data_columns.col_code);
    m_tree_view.append_column("Ярлык", m_data_columns.col_name);
    m_tree_view.append_column("Описание", m_data_columns.col_desc);
    m_tree_view.append_column("Ответcвенный", m_data_columns.col_owner);
    m_tree_view.append_column("Расположение", m_data_columns.col_loc);
    m_tree_view.append_column("Создан", m_data_columns.col_cdate);

    auto status_bar = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    status_bar->append(m_status_label);
    m_right_box.append(*status_bar);
}

void AppWindow::setup_actions()
{
    m_btn_add_data.signal_clicked().connect([this]() { AddDataWindow::show_as(*this); });
    m_btn_add_owner.signal_clicked().connect([this]() { AddOwnerWindow::show_as(*this); });
    m_btn_print.signal_clicked().connect(sigc::mem_fun(*this, &AppWindow::print_setup));
    m_btn_refresh.signal_clicked().connect(sigc::mem_fun(*this, &AppWindow::on_refresh));
    m_search_entry.signal_changed().connect(sigc::mem_fun(*this, &AppWindow::on_search_changed));
}

// void AppWindow::on_generate()
// {
//     int ownerSelected=-1;
//     int insertId;

//     std::string data = m_current_record.code;

//     display_image({});
//     if(data.empty())
//     {
//         if(DbManager::insert_type(m_current_record.type->short_code,ownerSelected,&insertId))
//             return;

//         int next_id = DbManager::get().get_next_auto_id();

//         int sel_model = m_typed_class_combo.get_active_row_number();
//         if(sel_model == -1)
//             return;

//         data = DbManager::get().get_types()[sel_model]->short_code + std::string(8 - std::to_string(next_id).length(), '0') + std::to_string(next_id);
//         m_code_label.set_text( data);
//     }

//     int error;
//     std::vector<unsigned char> m_current_image;
//     generate_barcode(data, get_symbology(m_type_combo.get_active_text()), error, m_current_image);
//     if(error != 0)
//     {
//         return;
//     }

//     display_image(m_current_image);
// }

void AppWindow::display_image(const Glib::RefPtr<Gdk::Pixbuf> &refPixBuf)
{
    if(!refPixBuf)
    {
        m_preview_picture.set_paintable(nullptr);
        return;
    }
    auto texture = Gdk::Texture::create_for_pixbuf(refPixBuf);
    m_preview_picture.set_paintable(texture);
}

void AppWindow::load_table()
{
    m_list_model->clear();
    std::vector<BarcodeRecord> records = DbManager::get().get_all();
    std::vector<Owner> owners = DbManager::get().get_owners();
    for(const BarcodeRecord &r : records)
    {
        auto row = *m_list_model->append();
        Owner owner {};
        owner.id = -1;
        auto iter = std::find_if(std::begin(owners), std::end(owners), [&r](const Owner &lhs) { return lhs.id == r.owner_id; });
        if(std::end(owners) != iter)
        {
            owner = *iter;
        }
        else
        {
            owner.name = owner.room = "(Не назначено)";
        }

        row[m_data_columns.col_id] = r.id;
        row[m_data_columns.col_code] = r.code;
        row[m_data_columns.col_name] = r.name;
        row[m_data_columns.col_desc] = r.description;
        row[m_data_columns.col_owner] = owner.name;
        row[m_data_columns.col_loc] = owner.room;
        row[m_data_columns.col_cdate] = r.created_at;
    }
}

void AppWindow::on_search_changed()
{
    m_list_model->clear();
    std::vector<BarcodeRecord> records = m_search_entry.get_text().empty() ? DbManager::get().get_all() : DbManager::get().search(m_search_entry.get_text());
    std::vector<Owner> owners = DbManager::get().get_owners();
    for(const BarcodeRecord &r : records)
    {
        auto row = *m_list_model->append();
        Owner owner {};
        owner.id = -1;

        auto iter = std::find_if(std::begin(owners), std::end(owners), [&r](const Owner &lhs) { return lhs.id == r.owner_id; });
        if(std::end(owners) != iter)
        {
            owner = *iter;
        }

        row[m_data_columns.col_id] = r.id;
        row[m_data_columns.col_code] = r.code;
        row[m_data_columns.col_name] = r.name;
        row[m_data_columns.col_desc] = r.description;
        row[m_data_columns.col_owner] = owner.name;
        row[m_data_columns.col_loc] = owner.room;
        row[m_data_columns.col_cdate] = r.created_at;
    }
}

void AppWindow::on_refresh()
{
    load_table();
}

void AppWindow::update_connection_status(bool connected)
{
    if(connected)
    {
        m_status_label.set_text("[+] MySQL подключен");
        m_status_label.set_name("connected");
    }
    else
    {
        m_status_label.set_text("[-] MySQL отключен");
        m_status_label.set_name("disconnected");
    }

    auto css = Gtk::CssProvider::create();
    css->load_from_data("#connected { color: #16a34a; } #disconnected { color: #dc2626; }");
    m_status_label.get_style_context()->add_provider(css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

void AppWindow::print_setup()
{
    std::vector<std::string> objects {};
    std::vector<int> ids {};
    Glib::RefPtr<const Gtk::TreeSelection> selection = m_tree_view.get_selection();
    std::vector<Gtk::TreeModel::Path> selected_paths = selection->get_selected_rows();

    for(const Gtk::TreeModel::Path &lhs : selected_paths)
    {
        Gtk::TreeModel::iterator iter = m_list_model->get_iter(lhs);
        if(iter)
        {
            Gtk::TreeRow &row = *iter;
            ids.push_back(row[m_data_columns.col_id]);
        }
    }

    objects = DbManager::get().get_all_code(std::move(ids));
    PrintSetupWindow::show_as(*this, std::move(objects));
}
