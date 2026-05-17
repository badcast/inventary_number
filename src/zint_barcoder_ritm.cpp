#include "zint_barcoder_ritm.h"

Glib::RefPtr<Gdk::Pixbuf> generate_barcode(const std::string &data, int type, bool show_text, int target_height_pt)
{
    struct zint_symbol *symbol = ZBarcode_Create();
    if(!symbol)
        return {};

    symbol->symbology = type;
    symbol->show_hrt = static_cast<int>(show_text);
    symbol->dpmm = 12;//150.0f / 25.4f; // DPI
    symbol->scale = 1;//ZBarcode_Scale_From_XdimDp(symbol->symbology, ZBarcode_Default_Xdim(symbol->symbology), symbol->dpmm, nullptr);
    symbol->height = target_height_pt;
    symbol->whitespace_width = 0;
    //symbol->output_options = BARCODE_BIND_TOP | BOLD_TEXT;

    int error = ZBarcode_Encode_and_Buffer(symbol, (unsigned char *) data.c_str(), 0, 0);
    if(error != 0)
    {
        ZBarcode_Delete(symbol);
        return {};
    }

    auto pixbuf = Gdk::Pixbuf::create_from_data(symbol->bitmap, Gdk::Colorspace::RGB, false, 8, symbol->bitmap_width, symbol->bitmap_height, symbol->bitmap_width * 3);
    ZBarcode_Delete(symbol);
    return pixbuf;
}

int get_symbology(const Glib::ustring &name)
{
    if(name == "CODE128")
        return BARCODE_CODE128;
    if(name == "EAN13")
        return BARCODE_EANX;
    if(name == "UPCA")
        return BARCODE_UPCA;
    if(name == "QRCODE")
        return BARCODE_QRCODE;
    if(name == "DATAMATRIX")
        return BARCODE_DATAMATRIX;
    if(name == "CODE39")
        return BARCODE_CODE39;
    if(name == "PDF417")
        return BARCODE_PDF417;
    if(name == "AZTEC")
        return BARCODE_AZTEC;
    return BARCODE_CODE128;
}
