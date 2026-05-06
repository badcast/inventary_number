#pragma once

#include <string>
#include <vector>
#include <zint.h>
#include <glibmm.h>
#include <gdkmm.h>

Glib::RefPtr<Gdk::Pixbuf> generate_barcode(const std::string &data, int type, bool show_text, int target_height_pt);
int get_symbology(const Glib::ustring &name);