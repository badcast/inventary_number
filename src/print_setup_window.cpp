#include "print_setup_window.h"
#include <cairomm/fontface.h>
#include <cairomm/surface.h>
#include <cairomm/fontface.h>

const std::vector<LabelPreset> PrintSetupWindow::m_presets = { {"40×25 мм", 4.0, 2.5}, {"50×30 мм", 5.0, 3.0}, {"70×35 мм", 7.0, 3.5}, {"100×50 мм", 10.0, 5.0}, {"Свой вариант", 5.0, 3.0}};

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
    window->signal_hide().connect([window]() { delete window; });
    window->soft_update_state();
    window->present();
    return window;
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

    int items_on_page = std::min(per_page, (int)m_data.size() - start_idx);
    if (items_on_page <= 0)
        return;

    if(m_cutline != CutLineType::NONE)
    {
        cr->save();
        cr->set_source_rgb(0.5, 0.5, 0.5);
        cr->set_line_width(0.3);
        cr->set_dash(std::vector<double> {2.0, 2.0}, 0);

        int actual_rows = (items_on_page + cols - 1) / cols;

        auto get_row_cols = [&](int r) {
            if (r < 0 || r >= actual_rows) return 0;
            return std::max(0, std::min(cols, items_on_page - r * cols));
        };

        auto get_col_rows = [&](int c) {
            if (c < 0 || c >= cols || items_on_page <= c) return 0;
            return (items_on_page - c - 1) / cols + 1;
        };

        if(m_cutline == CutLineType::HLINE || m_cutline == CutLineType::BOTH)
        {
            for(int r = 0; r <= actual_rows; ++r)
            {
                int line_cols = std::max(get_row_cols(r - 1), get_row_cols(r));
                if (line_cols > 0) {
                    double y = mt + r * ih_pt;
                    cr->move_to(ml, y);
                    cr->line_to(ml + line_cols * iw_pt, y);
                }
            }
            cr->stroke();
        }

        if(m_cutline == CutLineType::VLINE || m_cutline == CutLineType::BOTH)
        {
            for(int c = 0; c <= cols; ++c)
            {
                int line_rows = std::max(get_col_rows(c - 1), get_col_rows(c));
                if (line_rows > 0) {
                    double x = ml + c * iw_pt;
                    cr->move_to(x, mt);
                    cr->line_to(x, mt + line_rows * ih_pt);
                }
            }
            cr->stroke();
        }
        cr->restore();
    }

    Gdk::InterpType _interptype = Gdk::InterpType::NEAREST;
    double font_size = 10.0;
    double text_margin = 15.0;

    for(int i = 0; i < per_page; ++i)
    {
        int current_idx = start_idx + i;
        if(current_idx >= (int) m_data.size())
            break;

        bool show_text = m_chb_show_text.get_active();
        double actual_barcode_h = show_text ? (ih_pt - text_margin) : ih_pt;

        Glib::RefPtr<Gdk::Pixbuf> barcode_pixbuf = generate_barcode(m_data[current_idx], BARCODE_CODE128, false, (int)actual_barcode_h);

        if(barcode_pixbuf)
        {
            double orig_w = barcode_pixbuf->get_width();
            double orig_h = barcode_pixbuf->get_height();
            double scale = std::min(iw_pt / orig_w, actual_barcode_h / orig_h);

            int final_w = std::max(1, (int)(orig_w * scale));
            int final_h = std::max(1, (int)(orig_h * scale));

            Glib::RefPtr<Gdk::Pixbuf> scaled = barcode_pixbuf->scale_simple(final_w, final_h, _interptype);

            int c = i % cols;
            int r = i / cols;
            double cell_x = ml + c * iw_pt;
            double cell_y = mt + r * ih_pt;

            double x = std::round(cell_x + (iw_pt - final_w) / 2.0);
            double y;

            if (show_text)
                y = std::round(cell_y); // Начинаем от верхнего края ячейки (т.к. мы уже вычли место под текст)
            else
                y = std::round(cell_y + (ih_pt - final_h) / 2.0);

            Gdk::Cairo::set_source_pixbuf(cr, scaled, x, y);
            cr->paint();

            if(show_text)
            {
                cr->set_source_rgb(0, 0, 0);
                cr->select_font_face("Sans", Cairo::ToyFontFace::Slant::NORMAL, Cairo::ToyFontFace::Weight::NORMAL);
                cr->set_font_size(font_size);

                Cairo::TextExtents extents;
                cr->get_text_extents(m_data[current_idx], extents);

                // Центрируем текст по реальной ширине наклейки iw_pt
                double text_x = cell_x + (iw_pt - extents.width) / 2.0;
                double text_y = y + final_h + extents.height + 4.0;

                cr->move_to(text_x, text_y);
                cr->show_text(m_data[current_idx]);
            }
        }
    }
}

void PrintSetupWindow::draw_preview_page()
{
    if(m_data.empty())
        return;

    double pw = m_page_setup->get_paper_width(Gtk::Unit::POINTS);
    double ph = m_page_setup->get_paper_height(Gtk::Unit::POINTS);
    Cairo::RefPtr<Cairo::ImageSurface> surf = Cairo::ImageSurface::create(Cairo::Surface::Format::RGB24, (int) pw, (int) ph);
    Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(surf);

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

    m_right_box.set_margin(12);
    m_right_box.set_vexpand(true);
    m_right_box.set_spacing(12);
    m_spin_width_cm.set_value(m_item_width_cm);
    m_spin_height_cm.set_value(m_item_height_cm);
    m_combo_cutline.set_active(0);

    auto lbl_settings = Gtk::make_managed<Gtk::Label>("Настройки");
    lbl_settings->add_css_class("title-4");
    lbl_settings->set_halign(Gtk::Align::START);
    m_right_box.append(*lbl_settings);

    auto box_preset = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 3);
    auto lbl_preset = Gtk::make_managed<Gtk::Label>("Размер наклейки:");
    lbl_preset->set_xalign(0);
    box_preset->append(*lbl_preset);

    for(const LabelPreset &p : m_presets)
        m_combo_preset.append(p.name);
    m_combo_preset.set_active(0);
    m_combo_preset.signal_changed().connect(sigc::mem_fun(*this, &PrintSetupWindow::soft_update_state));
    box_preset->append(m_combo_preset);
    m_controls.append(*box_preset);

    auto box_size = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    m_spin_width_cm.set_range(1.0, 20.0);
    m_spin_width_cm.set_digits(1);
    m_spin_width_cm.set_increments(0.1, 1.0);
    m_spin_width_cm.set_width_chars(5);
    m_spin_width_cm.signal_value_changed().connect(sigc::mem_fun(*this, &PrintSetupWindow::soft_update_state));
    box_size->append(m_spin_width_cm);

    box_size->append(*Gtk::make_managed<Gtk::Label>("×"));

    m_spin_height_cm.set_range(1.0, 20.0);
    m_spin_height_cm.set_digits(1);
    m_spin_height_cm.set_increments(0.1, 1.0);
    m_spin_height_cm.set_width_chars(5);
    m_spin_height_cm.signal_value_changed().connect(sigc::mem_fun(*this, &PrintSetupWindow::soft_update_state));
    box_size->append(m_spin_height_cm);
    box_size->append(*Gtk::make_managed<Gtk::Label>("см"));
    m_controls.append(*box_size);

    auto box_cut = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 3);
    auto lbl_cut = Gtk::make_managed<Gtk::Label>("Линии обрезки:");
    lbl_cut->set_xalign(0);
    box_cut->append(*lbl_cut);

    m_combo_cutline.append("Нет");
    m_combo_cutline.append("Горизонтальные");
    m_combo_cutline.append("Вертикальные");
    m_combo_cutline.append("Гориз. + Вертик.");
    m_combo_cutline.set_active(m_combo_cutline.get_model()->children().size() - 1);
    m_combo_cutline.signal_changed().connect(sigc::mem_fun(*this, &PrintSetupWindow::soft_update_state));
    box_cut->append(m_combo_cutline);
    m_controls.append(*box_cut);

    m_chb_no_border.set_active(true);
    m_chb_no_border.signal_toggled().connect(sigc::mem_fun(*this, &PrintSetupWindow::soft_update_state));
    m_controls.append(m_chb_no_border);

    m_chb_show_text.set_active(true);
    m_chb_show_text.signal_toggled().connect(sigc::mem_fun(*this, &PrintSetupWindow::soft_update_state));
    m_controls.append(m_chb_show_text);

    m_right_box.append(m_controls);

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
    m_actions->add_action("close", sigc::mem_fun(*this, &PrintSetupWindow::close));
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
    soft_update_state();
}

void PrintSetupWindow::soft_update_state()
{
    if(m_updating_preset)
        return;
    m_updating_preset = true;
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

    m_cutline = static_cast<CutLineType>(m_combo_cutline.get_active_row_number());
    int idx = m_combo_preset.get_active_row_number();

    if(idx >= 0 && idx < (int) m_presets.size() - 1)
    {
        m_item_width_cm = m_presets[idx].width_cm;
        m_item_height_cm = m_presets[idx].height_cm;
        m_spin_width_cm.set_value(m_item_width_cm);
        m_spin_height_cm.set_value(m_item_height_cm);
    }
    else
        m_combo_preset.set_active(m_presets.size() - 1);

    m_item_width_cm = m_spin_width_cm.get_value();
    m_item_height_cm = m_spin_height_cm.get_value();

    m_pages_total = std::max(1, (int) std::ceil((double) m_dup / per_page));
    if(m_page >= m_pages_total)
        m_page = m_pages_total - 1;

    m_updating_preset = false;
    update_status();

    draw_preview_page();
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