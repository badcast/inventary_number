#include "db_manager.h"
#include <iostream>

std::vector<std::shared_ptr<Classification>> mClassifications;

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
        records.push_back(rec);
    }
    return records;
}

std::vector<std::string> DbManager::get_all_code(std::vector<int> ids)
{
    std::vector<std::string> records {};
    if(!conn || !conn->connected())
        return records;

    mysqlpp::Query query = conn->query();
    query << "SELECT code FROM inventory";
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

    for(const mysqlpp::Row &row : result)
    {
        records.push_back(row["code"].c_str());
    }
    return records;
}
std::vector<BarcodeRecord> DbManager::search(const std::string &query_str)
{
    std::vector<BarcodeRecord> records {};
    if(!conn || !conn->connected())
        return records;

    std::string match = "%" + query_str + "%";

    mysqlpp::Query query = conn->query();
    query << "SELECT inventory.* FROM inventory "
          << "INNER JOIN owners o ON o.id = inventory.owner_id "
          << "WHERE (inventory.code LIKE " << mysqlpp::quote << match << " OR inventory.name LIKE " << mysqlpp::quote << match << " OR o.name LIKE " << mysqlpp::quote << match << " OR o.room LIKE " << mysqlpp::quote << match << ") ORDER BY inventory.id DESC";
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
            rec.owner_id = row["owner_id"];
            rec.created_at = row["created_at"].c_str();
            rec.owned_at = row["owned_at"].c_str();
            records.push_back(std::move(rec));
        }
    }

    return records;
}
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

bool DbManager::insert_data(const std::string &type, const std::string &name, int ownerId, int *insertId)
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

    query << "INSERT INTO inventory (code,name,owner_id) VALUES (" << mysqlpp::quote << nextCode << ", " << mysqlpp::quote << name << ", ";
    if(ownerId == -1)
        query << "NULL";
    else
        query << ownerId;
    query << ")";
    if(!query.execute())
    {
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
