#include "db_manager.h"

#include <fstream>
#include <numeric>
#include <vector>
#include <string>
#include <string.h>
#include <iostream>

#include "fast_csv.h"

std::vector<std::shared_ptr<Classification>> mClassifications;

std::vector<std::shared_ptr<Classification>> DbManager::get_types()
{
    if(mClassifications.empty())
    {
        std::vector<Classification> data = {
                                             {"MO", "Мед. оборудование", "ЭКГ, УЗИ-аппараты, дефибрилляторы, мониторы пациента."},
                                             {"KT", "Компьютерная техника", "Системные блоки, серверы, ноутбуки, планшеты врачей."},
                                             {"KP", "Периферия и оргтехника", "Принтеры, сканеры, МФУ, мониторы, ИБП."},
                                             {"MB", "Мебель", "Столы, шкафы, медицинские кушетки, тумбы, кровати."},
                                             {"HI", "Хоз. инвентарь", "Сейфы, холодильники для лекарств, микроволновки, кондиционеры."},
                                             {"VS", "Видеоенаблюдение", "Камеры (IP/аналог), видеорегистраторы."},
                                             {"AV", "Аудио-визуальное", "Телевизоры в холлах, проекторы в конференц-залах."},
                                             {"TR", "Транспортировка", "Кресла-каталки, носилки, тележки для инструментов."},
                                             {"IT", "Расходные (дорогие)", "Специальные эргономичные мыши, сложные датчики (если нужен поштучный учет)."}};

        mClassifications.reserve(data.size());

        std::transform(std::make_move_iterator(data.begin()), std::make_move_iterator(data.end()), std::back_inserter(mClassifications), [](Classification &&c) { return std::make_shared<Classification>(std::move(c)); });
    }

    return mClassifications;
}

inline std::string toLower(std::string str)
{
    std::transform(std::begin(str), std::end(str), std::begin(str), [](unsigned char c) { return std::tolower(c); });
    return str;
}

inline std::string toUpper(std::string str)
{
    std::transform(std::begin(str), std::end(str), std::begin(str), [](unsigned char c) { return std::toupper(c); });
    return str;
}

inline std::string trimStart(const std::string &str)
{
    auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char ch) { return std::isspace(ch); });
    return std::string(start, str.end());
}

DbManager &DbManager::get()
{
    static DbManager instance;
    return instance;
}

bool DbManager::connect_db(const std::string &host, int port, const std::string &db, const std::string &user, const std::string &pass)
{
    disconnect();
    conn = new mysqlpp::Connection(false);

    if(!conn->connect(db.c_str(), host.c_str(), user.c_str(), pass.c_str(), port))
    {
        disconnect();
        signal_connection_changed.emit(false);
        return false;
    }

    create_tables();
    signal_connection_changed.emit(true);
    return true;
}

void DbManager::disconnect()
{
    if(conn)
    {
        conn->disconnect();
        delete conn;
        conn = nullptr;
        signal_connection_changed.emit(false);
    }
}

bool DbManager::create_tables()
{
    return conn->query(R"(
        CREATE TABLE IF NOT EXISTS owners (
            id           INT AUTO_INCREMENT PRIMARY KEY,
            name         VARCHAR(64) NOT NULL,
            room         VARCHAR(64),
            created_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
        );)")
               .execute() &&
           conn->query(R"(
        CREATE TABLE IF NOT EXISTS inventory (
            id           INT AUTO_INCREMENT PRIMARY KEY,
            code         VARCHAR(255) NOT NULL UNIQUE,
            name         VARCHAR(255) NOT NULL,
            name1        VARCHAR(255),
            name2        VARCHAR(255),
            owner_id     INT,
            owned_at     TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            created_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

            FOREIGN KEY  ownerid_fk(owner_id) REFERENCES owners (id)
        );)")
               .execute();
}

std::vector<Owner> DbManager::get_owners()
{
    std::vector<Owner> owners {};
    if(!conn || !conn->connected())
        return owners;

    mysqlpp::Query query = conn->query("SELECT id,name,room FROM owners ORDER BY id DESC");
    mysqlpp::StoreQueryResult result = query.store();
    if(!result)
        return owners;

    Owner own {};
    for(const mysqlpp::Row &row : result)
    {
        own.id = row["id"];
        own.name = row["name"].c_str();
        own.room = row["room"].c_str();
        owners.push_back(own);
    }

    return owners;
}

std::vector<BarcodeRecord> DbManager::get_all()
{
    std::vector<BarcodeRecord> records {};
    if(!conn || !conn->connected())
        return records;

    mysqlpp::Query query = conn->query("SELECT * FROM inventory ORDER BY id DESC");
    mysqlpp::StoreQueryResult result = query.store();
    if(!result)
        return records;

    BarcodeRecord rec;
    for(const mysqlpp::Row &row : result)
    {
        rec.id = row["id"];
        rec.name = row["name"].c_str();
        rec.type = classification_query(rec.code);

        rec.owner_id = row["owner_id"].is_null() ? -1 : row["owner_id"];
        rec.code = row["code"].c_str();
        rec.created_at = row["created_at"].c_str();
        rec.owned_at = row["owned_at"].c_str();
        rec.image_id = row["image_id"].is_null() ? -1 : row["image_id"];
        records.push_back(rec);
    }
    return records;
}

std::vector<MemberFieldData> DbManager::get_all_code(std::vector<int> ids)
{
    std::vector<MemberFieldData> records {};
    if(!conn || !conn->connected())
        return records;

    mysqlpp::Query query = conn->query();
    query << "SELECT code,name,name1 FROM inventory";
    if(!ids.empty())
    {
        query << " WHERE id IN (";
        for(int i = 0; i < ids.size(); ++i)
        {
            query << ids[i];
            if(i != ids.size() - 1)
            {
                query << ",";
            }
        }
        query << ")";
    }
    mysqlpp::StoreQueryResult result = query.store();
    if(!result)
        return records;

    MemberFieldData mfd;
    for(const mysqlpp::Row &row : result)
    {
        mfd.code = row["code"].c_str();
        mfd.name_ru = row["name"].c_str();
        mfd.name_en = row["name1"].c_str();
        records.push_back(mfd);
    }
    return records;
}
std::vector<BarcodeRecord> DbManager::search(const std::string &query_str)
{
    std::vector<BarcodeRecord> records {};
    if(!conn || !conn->connected())
        return records;

    std::string match = "%" + trimStart(query_str) + "%";
    mysqlpp::Query query = conn->query();
    query << "SELECT inventory.* FROM inventory "
          << "LEFT JOIN owners o ON o.id = inventory.owner_id "
          << "WHERE (inventory.code LIKE " << mysqlpp::quote << match << " OR inventory.name LIKE " << mysqlpp::quote << match << " OR o.name LIKE " << mysqlpp::quote << match << " OR o.room LIKE " << mysqlpp::quote << match << ") "
          << "ORDER BY inventory.id DESC";
    mysqlpp::StoreQueryResult result = query.store();

    if(result)
    {
        for(const auto &row : result)
        {
            BarcodeRecord rec;
            rec.id = row["id"];
            rec.code = row["code"].c_str();
            rec.name = row["name"].c_str();
            rec.type = classification_query(rec.code);
            rec.owner_id = row["owner_id"].is_null() ? -1 : row["owner_id"];
            rec.created_at = row["created_at"].c_str();
            rec.owned_at = row["owned_at"].c_str();
            records.push_back(std::move(rec));
        }
    }

    return records;
}

void DbManager::import_from_str(const std::string type, const std::string filename, int group, char column)
{
    struct t {
        std::string name0;
        std::string name1;
        int c = 0;
        std::string imageFileName;
    } t0;

    std::string line;
    std::vector<t> __i;

    FastCsvImporter fscv(column);

    fscv.import(filename, [&t0, &__i](const std::vector<std::string_view>& headers, const std::vector<std::string_view>& row){
           // En name
            t0.name0 = row[0];
            // Ru name
            t0.name1 = row[1];
            // Count
            t0.c = std::stoi(std::string(row[2]));
            // Imagefilename (image id)
            t0.imageFileName = "/tmp/media/";
            t0.imageFileName += row[3];
         __i.push_back(t0);
    });

    int total = std::accumulate(std::begin(__i),std::end(__i), 0, [](auto i, auto j){
        return i + std::abs(j.c);
    });

    for(const t& tt : __i)
    {
        int imageId = upload_image(tt.imageFileName);
        for(int i =0; i < tt.c; ++i)
        {
            int insertId;
            insert_data(type, tt.name1, tt.name0, -1, &insertId, group, imageId);
        }
    }
}

std::shared_ptr<Classification> DbManager::classification_query(std::string tp)
{
    std::shared_ptr<Classification> result {};
    if(tp.empty())
        return result;
    // get owned type
    if(tp.size() > 2)
        tp.resize(2);
    for(std::shared_ptr<Classification> &iter : mClassifications)
    {
        if(toUpper(iter->short_code) == toUpper(tp))
        {
            result = iter;
            break;
        }
    }

    return result;
}

bool DbManager::insert(const BarcodeRecord &rec)
{
    if(!conn || !conn->connected())
        return false;

    mysqlpp::Query query = conn->query("INSERT INTO inventory (code) VALUES (%0q)");
    query.parse();
    return query.execute(rec.code.c_str());
}

bool DbManager::insert_data(const std::string &type, const std::string &name, const std::string &name0, int ownerId, int *insertId, int group, int imageId)
{
    if(!conn || !conn->connected() || type.length() != 2 || name.length() < 3)
        return false;
    mysqlpp::Transaction trans(*conn, mysqlpp::Transaction::serializable, mysqlpp::Transaction::session);
    mysqlpp::Query query = conn->query();
    query << "SELECT CONCAT(" << mysqlpp::quote << type << ", "
          << "LPAD((SELECT COALESCE(MAX(CAST(SUBSTR(code, 3) AS UNSIGNED)), 0) + 1 FROM inventory WHERE code LIKE " << mysqlpp::quote << (type + "%") << "), 5, '0')"
          << ") AS next_code FROM (SELECT 1) AS t";

    mysqlpp::StoreQueryResult result = query.store();

    std::string nextCode;
    if(result && result.num_rows() > 0)
    {
        nextCode = result[0]["next_code"].c_str();
    }
    else
    {
        nextCode = toUpper(type) + "00001";
    }

    query.reset();

    query << "INSERT INTO inventory "
             "(code,name,name1,owner_id,type,image_id) VALUES (" << mysqlpp::quote << nextCode << ", " << mysqlpp::quote << name;
    query << ", ";
    if(!name0.empty())
        query << mysqlpp::quote << name0;
    else
        query << mysqlpp::quote << "";

    query << ",";

    if(ownerId == -1)
        query << "NULL";
    else
        query << ownerId;

    query << "," << group;

    query << "," << imageId;

    query << ")";

    if(!query.execute())
    {
        std::cerr << "Query failed: " << query.error() << std::endl;
        return false;
    }

    if(insertId != nullptr)
    {
        (*insertId) = static_cast<int>(query.insert_id());
    }

    trans.commit();
    return true;
}

bool DbManager::insert_owner(const std::string &name, const std::string &cabinet, int *insertId)
{
    if(!conn || !conn->connected() || name.length() < 3)
        return false;

    mysqlpp::Query query = conn->query("INSERT INTO owners (name,room) VALUES (%0q,%1q)");
    query.parse();
    mysqlpp::SimpleResult result = query.execute(name, cabinet);
    if(insertId != nullptr)
    {
        (*insertId) = result.insert_id();
    }
    return result;
}

bool DbManager::update_data(const BarcodeRecord &rec)
{
    if(!conn || !conn->connected())
        return false;

    mysqlpp::Query query = conn->query("UPDATE inventory SET code=%0q WHERE id=%1");
    query.parse();
    return query.execute(rec.code, rec.id);
}

bool DbManager::update_image(int id, Glib::RefPtr<Gdk::Pixbuf> picture)
{
    if(!conn || !conn->connected() || !picture)
        return false;

    gchar * buffer=  nullptr;
    gsize buffer_size = 0;
    try
    {
        picture->save_to_buffer(buffer,buffer_size, "jpeg");
    } catch (...)
    {
        return false;
    }

    mysqlpp::sql_blob blob(buffer, buffer_size);
    mysqlpp::Query query = conn->query("INSERT INTO images (data) VALUES (%0q)");
    query.parse();

    mysqlpp::SimpleResult result = query.execute(blob);

    if(!result)
    {
        g_free(buffer);
        return false;
    }

    query.reset();
    query = conn->query("UPDATE inventory SET image_id = %0 WHERE id = %1");
    query.parse();
    query.execute(result.insert_id(), id);

    return result;
}

int DbManager::upload_image(const std::string &filepath, int inventoryId)
{
    Glib::RefPtr<Gdk::Pixbuf> picture = Gdk::Pixbuf::create_from_file(filepath);

    gchar * buffer=  nullptr;
    gsize buffer_size = 0;
    try
    {
        picture->save_to_buffer(buffer,buffer_size, "jpeg");
    } catch (...)
    {
        return -1;
    }

    mysqlpp::sql_blob blob(buffer, buffer_size);
    mysqlpp::Query query = conn->query("INSERT INTO images (data) VALUES (%0q)");
    query.parse();

    mysqlpp::SimpleResult result = query.execute(blob);
    g_free(buffer);

    if(!result)
        return -1;

    int id = result.insert_id();

    if(inventoryId > -1)
    {
        query.reset();
        query = conn->query("UPDATE inventory SET image_id = %0 WHERE id = %1");
        query.parse();
        result = query.execute(id, inventoryId);
    }

    return id;
}

Glib::RefPtr<Gdk::Pixbuf> DbManager::get_image(int id)
{
    Glib::RefPtr<Gdk::Pixbuf> result {};

    if(!conn || !conn->connected())
        return result;

    mysqlpp::Query query = conn->query("SELECT g.data AS data FROM inventory i LEFT JOIN images g ON g.id = i.image_id WHERE i.id=%0");
    query.parse();
    mysqlpp::StoreQueryResult res = query.store(id);

    if(res && res.num_rows() > 0)
    {
        mysqlpp::Row row = res[0];
        if(!row["data"].is_null())
        {
            mysqlpp::String blob_data = row["data"];
            Glib::RefPtr<Gdk::PixbufLoader> loader = Gdk::PixbufLoader::create();
            loader->write(reinterpret_cast<const guint8*>(blob_data.data()), blob_data.size());
            loader->close();
            result = loader->get_pixbuf();
        }
    }

    return result;
}

bool DbManager::delete_data(int id)
{
    if(!conn || !conn->connected())
        return false;

    mysqlpp::Query query = conn->query("DELETE FROM inventory WHERE id=%0");
    query.parse();
    return query.execute(id);
}

void BarcodeRecord::setCode(std::string code)
{
    this->code = code;
    type = std::move(DbManager::get().classification_query(code));
}
