#include "print_setup_window.h"
#include <cairomm/fontface.h>
#include <cairomm/surface.h>

const std::vector<LabelPreset> PrintSetupWindow::m_presets = {{"50×30 мм", 5.0, 3.0}, {"40×25 мм", 4.0, 2.5}, {"70×35 мм", 7.0, 3.5}, {"100×50 мм", 10.0, 5.0}, {"Свой вариант", 5.0, 3.0}};

PrintSetupWindow::PrintSetupWindow(Gtk::Window &parent)
{
    set_title("Печать инвентарного номера");
    set_transient_for(parent);
    set_modal(true);
    set_default_size(1200, 800);

    auto key = Gtk::EventControllerKey::create();
    key->signal_key_pressed().connect(
        [this](guint k, guint, Gdk::ModifierType)
        {
            if(k == GDK_KEY_Escape)
            {
                close();
                return true;
            }
            return false;
        },
        false);
    add_controller(key);

    init_actions();
    init_ui();
    init_print();
}

PrintSetupWindow *PrintSetupWindow::show_as(Gtk::Window &parent, std::vector<std::string> data)
{
    if(data.empty())
    {
        auto dialog = Gtk::AlertDialog::create("Нет объектов для печати");
        dialog->set_detail("Выберите 1 или более объектов для печати.");
        dialog->show(parent);
        return nullptr;
    }

    auto window = new PrintSetupWindow(parent);
    window->m_data = std::move(data);
    window->m_dup = window->m_data.size();
    window->m_spin_dup.set_sensitive(false);
    window->m_spin_dup.set_value(window->m_dup);
    window->m_spin_dup.set_sensitive(true);

    window->m_spin_width_cm.signal_changed().freeze
    window->m_spin_width_cm.set_value(window->m_item_width_cm);
    window->m_spin_width_cm.set_sensitive(true);

    window->m_spin_height_cm.set_sensitive(false);
    window->m_spin_height_cm.set_value(window->m_item_height_cm);
    window->m_spin_height_cm.set_sensitive(true);

    window->update_status();

    window->signal_hide().connect([window]() { delete window; });
    window->present();
    return window;
}
void inspect_pixel_data(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf) {
    if (!pixbuf) return;

    int width = pixbuf->get_width();
    int height = pixbuf->get_height();
    int n_channels = pixbuf->get_n_channels();
    int rowstride = pixbuf->get_rowstride();
    unsigned char* pixels = pixbuf->get_pixels();

    if (!pixels) {
        std::cout << "Pixel data is NULL!" << std::endl;
        return;
    }

    // Проверяем первый пиксель (0,0)
    int offset = 0; // row 0, col 0
    std::cout << "First pixel (0,0):" << std::endl;
    if (n_channels >= 3) {
        std::cout << "  R: " << (int)pixels[offset + 0] << std::endl;
        std::cout << "  G: " << (int)pixels[offset + 1] << std::endl;
        std::cout << "  B: " << (int)pixels[offset + 2] << std::endl;
        if (n_channels == 4) {
            std::cout << "  A: " << (int)pixels[offset + 3] << std::endl;
        }
    }

    // Проверяем центральный пиксель
    int center_x = width / 2;
    int center_y = height / 2;
    int center_offset = center_y * rowstride + center_x * n_channels;
    std::cout << "\nCenter pixel (" << center_x << "," << center_y << "):" << std::endl;
    if (n_channels >= 3) {
        std::cout << "  R: " << (int)pixels[center_offset + 0] << std::endl;
        std::cout << "  G: " << (int)pixels[center_offset + 1] << std::endl;
        std::cout << "  B: " << (int)pixels[center_offset + 2] << std::endl;
        if (n_channels == 4) {
            std::cout << "  A: " << (int)pixels[center_offset + 3] << std::endl;
        }
    }
}


void PrintSetupWindow::generate_page_content(Cairo::RefPtr<Cairo::Context> cr, int page_nr, double width_pt, double height_pt, bool quality)
{
    if(m_data.empty())
        return;

    cr->set_source_rgb(1, 1, 1);
    cr->paint();

    double ml = m_page_setup->get_left_margin(Gtk::Unit::POINTS);
    double mt = m_page_setup->get_top_margin(Gtk::Unit::POINTS);
    double mr = m_page_setup->get_right_margin(Gtk::Unit::POINTS);
    double mb = m_page_setup->get_bottom_margin(Gtk::Unit::POINTS);

    double cw = width_pt - ml - mr;
    double ch = height_pt - mt - mb;
    double iw_pt = m_item_width_cm * CM_TO_PT;
    double ih_pt = m_item_height_cm * CM_TO_PT;

    int cols = std::max(1, (int) (cw / iw_pt));
    int rows = std::max(1, (int) (ch / ih_pt));
    int per_page = cols * rows;
    int start_idx = page_nr * per_page;

    if(m_cutline != CutLineType::NONE)
    {
        cr->save();
        cr->set_source_rgb(0.5, 0.5, 0.5);
        cr->set_line_width(0.3);
        cr->set_dash(std::vector<double> {2.0, 2.0}, 0);

        if(m_cutline == CutLineType::HLINE || m_cutline == CutLineType::BOTH)
        {
            for(int r = 0; r <= rows; ++r)
            {
                double y = mt + r * (ch / rows);
                cr->move_to(ml, y);
                cr->line_to(ml + cw, y);
            }
            cr->stroke();
        }

        if(m_cutline == CutLineType::VLINE || m_cutline == CutLineType::BOTH)
        {
            for(int c = 0; c <= cols; ++c)
            {
                double x = ml + c * (cw / cols);
                cr->move_to(x, mt);
                cr->line_to(x, mt + ch);
            }
            cr->stroke();
        }
        cr->restore();
    }

    Gdk::InterpType _interptype = quality ? Gdk::InterpType::HYPER : Gdk::InterpType::NEAREST;
    for(int i = 0; i < per_page; ++i)
    {
        int current_idx = start_idx + i;
        if(current_idx >= (int) m_data.size())
            break;

        Glib::RefPtr<Gdk::Pixbuf> barcode_pixbuf = generate_barcode(m_data[current_idx], BARCODE_CODE128, m_chb_show_text.get_active(), (int) m_item_height_cm);

        if(barcode_pixbuf)
        {
            int c = i % cols;
            int r = i / cols;
            double x = std::round(ml + c * (cw / cols) + (cw / cols - iw_pt) / 2);
            double y = std::round(mt + r * (ch / rows) + (ch / rows - ih_pt) / 2);
            Glib::RefPtr<Gdk::Pixbuf> scaled = barcode_pixbuf->scale_simple((int) iw_pt, (int) ih_pt, _interptype);
            Gdk::Cairo::set_source_pixbuf(cr,scaled, x, y);
            cr->paint();
        }
    }
}

void PrintSetupWindow::draw_preview_page()
{
    if(m_data.empty())
        return;

    double pw = m_page_setup->get_paper_width(Gtk::Unit::POINTS);
    double ph = m_page_setup->get_paper_height(Gtk::Unit::POINTS);

    auto surf = Cairo::ImageSurface::create(Cairo::Surface::Format::RGB24, (int) pw, (int) ph);
    auto cr = Cairo::Context::create(surf);

    generate_page_content(cr, m_page, pw, ph, false);

    auto pixbuf_preview = Gdk::Pixbuf::create(surf, 0, 0, surf->get_width(), surf->get_height());
    m_preview_page.set_pixbuf(pixbuf_preview);

    double scale_factor = 0.5;
    m_preview_frame.set_size_request((int) (pw * scale_factor), (int) (ph * scale_factor));

    m_lbl_page.set_text("Стр. " + std::to_string(m_page + 1) + " из " + std::to_string(m_pages_total));
    m_btn_prev.set_sensitive(m_page > 0);
    m_btn_next.set_sensitive(m_page < m_pages_total - 1);
}

void PrintSetupWindow::draw_print_page(const Glib::RefPtr<Gtk::PrintContext> &context, int page_nr)
{
    auto cr = context->get_cairo_context();
    double width = context->get_width();
    double height = context->get_height();

    generate_page_content(cr, page_nr, width, height, true);
}

void PrintSetupWindow::init_ui()
{
    set_child(m_box);
    create_menubar();
    create_toolbar();

    m_box.append(m_paned);
    m_paned.set_vexpand(true);
    m_paned.set_wide_handle(true);
    m_paned.set_position(700);

    m_left_box.set_margin(12);
    m_left_box.set_vexpand(true);

    auto lbl_preview = Gtk::make_managed<Gtk::Label>("Предпросмотр");
    lbl_preview->add_css_class("title-4");
    lbl_preview->set_halign(Gtk::Align::START);
    m_left_box.append(*lbl_preview);

    m_bg_area.set_draw_func(
        [this](const Glib::RefPtr<Cairo::Context> &cr, int w, int h)
        {
            cr->set_source_rgb(0.85, 0.85, 0.85);
            cr->paint();
        });
    m_bg_area.set_hexpand(true);
    m_bg_area.set_vexpand(true);

    auto overlay = Gtk::make_managed<Gtk::Overlay>();
    overlay->set_child(m_bg_area);

    m_scroll_preview.set_vexpand(true);
    m_scroll_preview.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);

    m_preview_frame.add_css_class("view");
    m_preview_frame.set_halign(Gtk::Align::CENTER);
    m_preview_frame.set_valign(Gtk::Align::CENTER);

    m_preview_page.set_content_fit(Gtk::ContentFit::CONTAIN);
    m_preview_page.set_can_shrink(true);
    m_preview_page.set_expand(true);

    m_preview_frame.set_child(m_preview_page);
    m_scroll_preview.set_child(m_preview_frame);
    overlay->add_overlay(m_scroll_preview);
    m_left_box.append(*overlay);

    // Навигация
    m_nav.set_halign(Gtk::Align::CENTER);
    m_nav.set_margin_top(6);
    m_nav.set_spacing(12);

    m_btn_prev.add_css_class("flat");
    m_btn_prev.add_css_class("circular");
    m_btn_prev.set_icon_name("go-previous-symbolic");
    m_btn_prev.signal_clicked().connect(sigc::mem_fun(*this, &PrintSetupWindow::on_prev_page));
    m_nav.append(m_btn_prev);

    m_lbl_page.set_width_chars(15);
    m_nav.append(m_lbl_page);

    m_btn_next.add_css_class("flat");
    m_btn_next.add_css_class("circular");
    m_btn_next.set_icon_name("go-next-symbolic");
    m_btn_next.signal_clicked().connect(sigc::mem_fun(*this, &PrintSetupWindow::on_next_page));
    m_nav.append(m_btn_next);

    m_left_box.append(m_nav);
    m_paned.set_start_child(m_left_box);

    // Правая панель (Настройки)
    m_right_box.set_margin(12);
    m_right_box.set_vexpand(true);
    m_right_box.set_spacing(12);

    auto lbl_settings = Gtk::make_managed<Gtk::Label>("Настройки");
    lbl_settings->add_css_class("title-4");
    lbl_settings->set_halign(Gtk::Align::START);
    m_right_box.append(*lbl_settings);

    // Счётчик объектов
    auto box_dup = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    auto lbl_dup = Gtk::make_managed<Gtk::Label>("Объектов:");
    lbl_dup->set_xalign(0);
    lbl_dup->set_hexpand(true);
    box_dup->append(*lbl_dup);

    m_spin_dup.set_range(1, 1000);
    m_spin_dup.set_sensitive(false);
    m_spin_dup.set_width_chars(4);
    m_spin_dup.add_css_class("flat");
    box_dup->append(m_spin_dup);
    m_controls.append(*box_dup);

    // Пресеты
    auto box_preset = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 3);
    auto lbl_preset = Gtk::make_managed<Gtk::Label>("Размер наклейки:");
    lbl_preset->set_xalign(0);
    box_preset->append(*lbl_preset);

    for(const auto &p : m_presets)
    {
        m_combo_preset.append(p.name);
    }
    m_combo_preset.signal_changed().connect(sigc::mem_fun(*this, &PrintSetupWindow::on_preset_changed));
    box_preset->append(m_combo_preset);
    m_controls.append(*box_preset);

    // Ручной размер
    auto box_size = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    m_spin_width_cm.set_range(1.0, 20.0);
    m_spin_width_cm.set_digits(1);
    m_spin_width_cm.set_increments(0.1, 1.0);
    m_spin_width_cm.set_width_chars(5);
    m_spin_width_cm.signal_value_changed().connect(sigc::mem_fun(*this, &PrintSetupWindow::on_size_cm_changed));
    box_size->append(m_spin_width_cm);

    box_size->append(*Gtk::make_managed<Gtk::Label>("×"));

    m_spin_height_cm.set_range(1.0, 20.0);
    m_spin_height_cm.set_digits(1);
    m_spin_height_cm.set_increments(0.1, 1.0);
    m_spin_height_cm.set_width_chars(5);
    m_spin_height_cm.signal_value_changed().connect(sigc::mem_fun(*this, &PrintSetupWindow::on_size_cm_changed));
    box_size->append(m_spin_height_cm);
    box_size->append(*Gtk::make_managed<Gtk::Label>("см"));
    m_controls.append(*box_size);

    // Линии обрезки
    auto box_cut = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 3);
    auto lbl_cut = Gtk::make_managed<Gtk::Label>("Линии обрезки:");
    lbl_cut->set_xalign(0);
    box_cut->append(*lbl_cut);

    m_combo_cutline.append("Нет");
    m_combo_cutline.append("Горизонтальные");
    m_combo_cutline.append("Вертикальные");
    m_combo_cutline.append("Гориз. + Вертик.");
    m_combo_cutline.set_active(m_combo_cutline.get_model()->children().size() - 1);
    m_combo_cutline.signal_changed().connect(sigc::mem_fun(*this, &PrintSetupWindow::on_preset_changed));
    box_cut->append(m_combo_cutline);
    m_controls.append(*box_cut);

    // Чекбоксы
    m_chb_no_border.set_active(true);
    m_chb_no_border.signal_toggled().connect(sigc::mem_fun(*this, &PrintSetupWindow::on_preset_changed));
    m_controls.append(m_chb_no_border);

    m_chb_show_text.set_active(true);
    m_chb_show_text.signal_toggled().connect(sigc::mem_fun(*this, &PrintSetupWindow::on_preset_changed));
    m_controls.append(m_chb_show_text);

    m_right_box.append(m_controls);

    // Кнопка печати
    m_bottom_box.set_halign(Gtk::Align::END);
    m_bottom_box.set_valign(Gtk::Align::END);
    m_bottom_box.set_vexpand(true);
    m_bottom_box.set_margin_top(24);

    m_btn_print.add_css_class("suggested-action");
    m_btn_print.add_css_class("pill");
    m_btn_print.set_size_request(180, 50);
    m_btn_print.set_label("Печать");
    m_btn_print.signal_clicked().connect(sigc::mem_fun(*this, &PrintSetupWindow::on_print));
    m_bottom_box.append(m_btn_print);

    m_right_box.append(m_bottom_box);
    m_paned.set_end_child(m_right_box);

    create_statusbar();
}

void PrintSetupWindow::create_menubar()
{
    auto menu = Gio::Menu::create();
    auto file = Gio::Menu::create();
    file->append("Параметры страницы...", "win.page_setup");
    file->append("Печать...", "win.print");
    file->append("Закрыть", "win.close");
    menu->append_submenu("Файл", file);
    m_menubar.set_menu_model(menu);
    m_box.append(m_menubar);
}

void PrintSetupWindow::create_toolbar()
{
    m_toolbar.add_css_class("toolbar");
    m_toolbar.set_margin(6);
    m_toolbar.set_spacing(6);

    auto btn_print = Gtk::make_managed<Gtk::Button>();
    btn_print->add_css_class("flat");
    btn_print->set_icon_name("document-print-symbolic");
    btn_print->set_action_name("win.print");
    m_toolbar.append(*btn_print);

    auto btn_setup = Gtk::make_managed<Gtk::Button>();
    btn_setup->add_css_class("flat");
    btn_setup->set_icon_name("document-page-setup-symbolic");
    btn_setup->set_action_name("win.page_setup");
    m_toolbar.append(*btn_setup);

    m_box.append(m_toolbar);
}

void PrintSetupWindow::create_statusbar()
{
    m_box.append(m_statusbar);
}

void PrintSetupWindow::init_actions()
{
    m_actions = Gio::SimpleActionGroup::create();
    m_actions->add_action("print", sigc::mem_fun(*this, &PrintSetupWindow::on_print));
    m_actions->add_action("page_setup", sigc::mem_fun(*this, &PrintSetupWindow::on_page_setup));
    m_actions->add_action("close", sigc::mem_fun(*this, &PrintSetupWindow::on_close));
    insert_action_group("win", m_actions);

    if(auto app = get_application())
    {
        app->set_accel_for_action("win.print", "<Ctrl>P");
        app->set_accel_for_action("win.close", "<Ctrl>W");
    }
}

void PrintSetupWindow::init_print()
{
    m_page_setup = Gtk::PageSetup::create();
    m_page_setup->set_orientation(Gtk::PageOrientation::PORTRAIT);
    m_page_setup->set_paper_size(Gtk::PaperSize(Gtk::PAPER_NAME_A4));
    m_print_settings = Gtk::PrintSettings::create();
    m_print_settings->set_resolution(600);
}

void PrintSetupWindow::on_print()
{
    auto op = Gtk::PrintOperation::create();
    op->set_print_settings(m_print_settings);
    op->set_default_page_setup(m_page_setup);
    op->set_n_pages(m_pages_total);
    op->signal_draw_page().connect([this](const Glib::RefPtr<Gtk::PrintContext> &context, int page_nr) { draw_print_page(context, page_nr); });

    try
    {
        auto result = op->run(Gtk::PrintOperation::Action::PRINT_DIALOG, *this);
        if(result == Gtk::PrintOperation::Result::APPLY)
        {
            m_print_settings = op->get_print_settings();
            m_statusbar.push("Печать запущена", 0);
        }
    }
    catch(const Gtk::PrintError &ex)
    {
        m_statusbar.push("Ошибка: " + Glib::ustring(ex.what()), 0);
    }
}

void PrintSetupWindow::on_page_setup()
{
    auto new_setup = Gtk::run_page_setup_dialog(*this, m_page_setup, m_print_settings);
    m_page_setup = new_setup;
    m_page = 0;
    update_pagination();
    draw_preview_page();
    update_status();
}

void PrintSetupWindow::update_pagination()
{
    double pw = m_page_setup->get_paper_width(Gtk::Unit::POINTS);
    double ph = m_page_setup->get_paper_height(Gtk::Unit::POINTS);

    if(m_chb_no_border.get_active())
    {
        m_page_setup->set_top_margin(0.0, Gtk::Unit::MM);
        m_page_setup->set_bottom_margin(0.0, Gtk::Unit::MM);
        m_page_setup->set_left_margin(0.0, Gtk::Unit::MM);
        m_page_setup->set_right_margin(0.0, Gtk::Unit::MM);
    }
    else
    {
        m_page_setup->set_top_margin(3.0, Gtk::Unit::MM);
        m_page_setup->set_bottom_margin(3.0, Gtk::Unit::MM);
        m_page_setup->set_left_margin(3.0, Gtk::Unit::MM);
        m_page_setup->set_right_margin(3.0, Gtk::Unit::MM);
    }

    double ml = m_page_setup->get_left_margin(Gtk::Unit::POINTS);
    double mt = m_page_setup->get_top_margin(Gtk::Unit::POINTS);
    double mr = m_page_setup->get_right_margin(Gtk::Unit::POINTS);
    double mb = m_page_setup->get_bottom_margin(Gtk::Unit::POINTS);

    double cw = pw - ml - mr;
    double ch = ph - mt - mb;
    double iw_pt = m_item_width_cm * CM_TO_PT;
    double ih_pt = m_item_height_cm * CM_TO_PT;

    int cols = std::max(1, (int) (cw / iw_pt));
    int rows = std::max(1, (int) (ch / ih_pt));
    int per_page = cols * rows;

    m_pages_total = std::max(1, (int) std::ceil((double) m_dup / per_page));
    if(m_page >= m_pages_total)
        m_page = m_pages_total - 1;
}

void PrintSetupWindow::on_preset_changed()
{
    if(m_updating_preset)
        return;

    m_cutline = static_cast<CutLineType>(m_combo_cutline.get_active_row_number());
    int idx = m_combo_preset.get_active_row_number();

    if(idx >= 0 && idx < (int) m_presets.size() - 1)
    {
        m_updating_preset = true;
        m_item_width_cm = m_presets[idx].width_cm;
        m_item_height_cm = m_presets[idx].height_cm;
        m_spin_width_cm.set_value(m_item_width_cm);
        m_spin_height_cm.set_value(m_item_height_cm);
        m_updating_preset = false;
    }

    update_pagination();
    draw_preview_page();
    update_status();
}

void PrintSetupWindow::on_size_cm_changed()
{
    if(m_updating_preset)
        return;

    m_item_width_cm = m_spin_width_cm.get_value();
    m_item_height_cm = m_spin_height_cm.get_value();

    m_updating_preset = true;
    m_combo_preset.set_active(m_presets.size() - 1);
    m_updating_preset = false;

    update_pagination();
    draw_preview_page();
    update_status();
}

void PrintSetupWindow::on_close()
{
    close();
}

void PrintSetupWindow::on_prev_page()
{
    if(m_page > 0)
    {
        m_page--;
        draw_preview_page();
    }
}

void PrintSetupWindow::on_next_page()
{
    if(m_page < m_pages_total - 1)
    {
        m_page++;
        draw_preview_page();
    }
}

void PrintSetupWindow::update_status()
{
    m_statusbar.pop(0);
    auto orient = m_page_setup->get_orientation() == Gtk::PageOrientation::PORTRAIT ? "Книжная" : "Альбомная";
    m_statusbar.push("Объектов: " + std::to_string(m_dup) + " | Размер: " + std::to_string(m_item_width_cm) + "×" + std::to_string(m_item_height_cm) + "см" + " | " + orient, 0);
}