
#pragma once
#include <gdkmm/pixbuf.h>
#include <gdkmm/pixbufloader.h>
#include <glibmm/refptr.h>
#include <string>
#include <cstring>
#include <vector>
#include <memory>
#include <mysql++/mysql++.h>
#include <sigc++/sigc++.h>

struct Owner
{
    int id;
    std::string name;
    std::string room;
};

struct MemberFieldData{
    std::string code;
    std::string name_ru;
    std::string name_en;
};

struct Classification
{
    std::string short_code;
    std::string name;
    std::string desc;

    Classification(std::string m0, std::string m1, std::string m2) : short_code(m0), name(m1), desc(m2)
    {
    }
};

struct BarcodeRecord
{
    int id = 0;
    int owner_id;
    int image_id;
    std::string code;
    std::string name;
    std::shared_ptr<Classification> type;
    std::string owned_at;
    std::string created_at;
    std::string updated_at;
    std::string description;

    void setCode(std::string code);

    inline std::string onlyCode() const
    {
        if(code.length() > 2 && std::isalnum(static_cast<unsigned char>(code[0])) && std::isalnum(static_cast<unsigned char>(code[1])))
        {
            return code.substr(2);
        }
        return code;
    }
};

class DbManager
{
public:
    static DbManager &get();

    bool connect_db(const std::string &host, int port, const std::string &db, const std::string &user, const std::string &pass);
    void disconnect();
    bool is_connected() const
    {
        return conn != nullptr;
    }

    bool create_tables();
    std::vector<Owner> get_owners();
    std::vector<BarcodeRecord> get_all();
    std::vector<MemberFieldData> get_all_code(std::vector<int> ids);
    std::vector<BarcodeRecord> search(const std::string &query);
    std::vector<std::shared_ptr<Classification>> get_types();
    void import_from_str(const std::string type, const std::string filename, int group, char column = ',');

    std::shared_ptr<Classification> classification_query(std::string tp);
    bool insert(const BarcodeRecord &rec);
    bool insert_data(const std::string &type, const std::string &name, const std::string &name0 = {}, int ownerId = -1, int *insertId = nullptr, int group = 0, int imageId = -1);
    bool insert_owner(const std::string &name, const std::string &cabinet, int *insertId = nullptr);
    bool update_data(const BarcodeRecord &rec);
    bool update_image(int id, Glib::RefPtr<Gdk::Pixbuf> picture);
    int upload_image(const std::string& filepath, int inventoryId = -1);
    Glib::RefPtr<Gdk::Pixbuf> get_image(int id);
    bool delete_data(int id);

    sigc::signal<void(bool)> signal_connection_changed;

private:
    DbManager() = default;
    ~DbManager()
    {
        disconnect();
    }
    mysqlpp::Connection *conn = nullptr;

    std::vector<std::shared_ptr<Classification>> _classify;
};
