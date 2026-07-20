#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>

class AstraDatabase {
private:
    sqlite3* db = nullptr;

    void execute_query(const std::string& sql) {
        char* err_msg = nullptr;
        int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL Error: " << err_msg << std::endl;
            sqlite3_free(err_msg);
        }
    }

public:
    AstraDatabase(const std::string& path = "astra.db") {
        int rc = sqlite3_open(path.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
            db = nullptr;
        } else {
            create_tables();
        }
    }

    ~AstraDatabase() {
        if (db) {
            sqlite3_close(db);
        }
    }

    void create_tables() {
        // Create users table
        std::string create_users = 
            "CREATE TABLE IF NOT EXISTS users ("
            "uuid TEXT PRIMARY KEY,"
            "username TEXT UNIQUE,"
            "email TEXT UNIQUE,"
            "password TEXT,"
            "points INTEGER DEFAULT 1250,"
            "badges TEXT DEFAULT '[\"Founder\"]',"
            "cosmetics TEXT DEFAULT '[]',"
            "equipped_cape TEXT DEFAULT '',"
            "equipped_wings TEXT DEFAULT '',"
            "equipped_hat TEXT DEFAULT '',"
            "equipped_aura TEXT DEFAULT '',"
            "skin_url TEXT DEFAULT '',"
            "cape_url TEXT DEFAULT ''"
            ");";
        execute_query(create_users);

        // Create sessions table
        std::string create_sessions = 
            "CREATE TABLE IF NOT EXISTS sessions ("
            "token TEXT PRIMARY KEY,"
            "client_token TEXT,"
            "uuid TEXT,"
            "FOREIGN KEY(uuid) REFERENCES users(uuid)"
            ");";
        execute_query(create_sessions);
    }

    struct User {
        std::string uuid;
        std::string username;
        std::string email;
        std::string password;
        int points = 0;
        std::string badges;
        std::string cosmetics;
        std::string equipped_cape;
        std::string equipped_wings;
        std::string equipped_hat;
        std::string equipped_aura;
        std::string skin_url;
        std::string cape_url;
    };

    bool user_exists(const std::string& email_or_username) {
        std::string query = "SELECT COUNT(*) FROM users WHERE email = ? OR username = ?;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, email_or_username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, email_or_username.c_str(), -1, SQLITE_TRANSIENT);

        bool exists = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exists = sqlite3_column_int(stmt, 0) > 0;
        }
        sqlite3_finalize(stmt);
        return exists;
    }

    bool register_user(const std::string& uuid, const std::string& username, const std::string& email, const std::string& password) {
        std::string query = "INSERT INTO users (uuid, username, email, password) VALUES (?, ?, ?, ?);";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, uuid.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, email.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, password.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    bool get_user(const std::string& email_or_username, User& user) {
        std::string query = "SELECT uuid, username, email, password, points, badges, cosmetics, "
                            "equipped_cape, equipped_wings, equipped_hat, equipped_aura, skin_url, cape_url "
                            "FROM users WHERE email = ? OR username = ?;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, email_or_username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, email_or_username.c_str(), -1, SQLITE_TRANSIENT);

        bool found = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            user.uuid = (const char*)sqlite3_column_text(stmt, 0);
            user.username = (const char*)sqlite3_column_text(stmt, 1);
            user.email = (const char*)sqlite3_column_text(stmt, 2);
            user.password = (const char*)sqlite3_column_text(stmt, 3);
            user.points = sqlite3_column_int(stmt, 4);
            user.badges = (const char*)sqlite3_column_text(stmt, 5);
            user.cosmetics = (const char*)sqlite3_column_text(stmt, 6);
            user.equipped_cape = (const char*)sqlite3_column_text(stmt, 7);
            user.equipped_wings = (const char*)sqlite3_column_text(stmt, 8);
            user.equipped_hat = (const char*)sqlite3_column_text(stmt, 9);
            user.equipped_aura = (const char*)sqlite3_column_text(stmt, 10);
            user.skin_url = (const char*)sqlite3_column_text(stmt, 11);
            user.cape_url = (const char*)sqlite3_column_text(stmt, 12);
            found = true;
        }
        sqlite3_finalize(stmt);
        return found;
    }

    bool get_user_by_uuid(const std::string& uuid, User& user) {
        std::string query = "SELECT uuid, username, email, password, points, badges, cosmetics, "
                            "equipped_cape, equipped_wings, equipped_hat, equipped_aura, skin_url, cape_url "
                            "FROM users WHERE uuid = ?;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, uuid.c_str(), -1, SQLITE_TRANSIENT);

        bool found = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            user.uuid = (const char*)sqlite3_column_text(stmt, 0);
            user.username = (const char*)sqlite3_column_text(stmt, 1);
            user.email = (const char*)sqlite3_column_text(stmt, 2);
            user.password = (const char*)sqlite3_column_text(stmt, 3);
            user.points = sqlite3_column_int(stmt, 4);
            user.badges = (const char*)sqlite3_column_text(stmt, 5);
            user.cosmetics = (const char*)sqlite3_column_text(stmt, 6);
            user.equipped_cape = (const char*)sqlite3_column_text(stmt, 7);
            user.equipped_wings = (const char*)sqlite3_column_text(stmt, 8);
            user.equipped_hat = (const char*)sqlite3_column_text(stmt, 9);
            user.equipped_aura = (const char*)sqlite3_column_text(stmt, 10);
            user.skin_url = (const char*)sqlite3_column_text(stmt, 11);
            user.cape_url = (const char*)sqlite3_column_text(stmt, 12);
            found = true;
        }
        sqlite3_finalize(stmt);
        return found;
    }

    bool update_cosmetics(const std::string& uuid, const std::string& cosmetics_json) {
        std::string query = "UPDATE users SET cosmetics = ? WHERE uuid = ?;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, cosmetics_json.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, uuid.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    bool update_equipped(const std::string& uuid, const std::string& slot, const std::string& item_name) {
        std::string column = "";
        if (slot == "cape") column = "equipped_cape";
        else if (slot == "wings") column = "equipped_wings";
        else if (slot == "hat") column = "equipped_hat";
        else if (slot == "aura") column = "equipped_aura";
        else return false;

        std::string query = "UPDATE users SET " + column + " = ? WHERE uuid = ?;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, item_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, uuid.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    bool update_textures(const std::string& uuid, const std::string& skin_url, const std::string& cape_url) {
        std::string query = "UPDATE users SET skin_url = ?, cape_url = ? WHERE uuid = ?;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, skin_url.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, cape_url.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, uuid.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    bool update_points(const std::string& uuid, int delta) {
        std::string query = "UPDATE users SET points = points + ? WHERE uuid = ?;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;

        sqlite3_bind_int(stmt, 1, delta);
        sqlite3_bind_text(stmt, 2, uuid.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    bool update_badges(const std::string& uuid, const std::string& badges_json) {
        std::string query = "UPDATE users SET badges = ? WHERE uuid = ?;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, badges_json.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, uuid.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    // Session Management
    bool create_session(const std::string& token, const std::string& client_token, const std::string& uuid) {
        // Remove old sessions for this user first
        std::string clean_query = "DELETE FROM sessions WHERE uuid = ?;";
        sqlite3_stmt* clean_stmt;
        if (sqlite3_prepare_v2(db, clean_query.c_str(), -1, &clean_stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(clean_stmt, 1, uuid.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(clean_stmt);
            sqlite3_finalize(clean_stmt);
        }

        std::string query = "INSERT INTO sessions (token, client_token, uuid) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, client_token.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, uuid.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    bool validate_session(const std::string& token, std::string& uuid) {
        std::string query = "SELECT uuid FROM sessions WHERE token = ?;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);

        bool valid = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            uuid = (const char*)sqlite3_column_text(stmt, 0);
            valid = true;
        }
        sqlite3_finalize(stmt);
        return valid;
    }

    bool remove_session(const std::string& token) {
        std::string query = "DELETE FROM sessions WHERE token = ?;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }
};
