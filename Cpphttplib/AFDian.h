
#ifndef AUTH_AFDIAN_H
#define AUTH_AFDIAN_H

#include <string>
#include <utility>
#include "nlohmann/json.hpp"

class AFDian {
private:
    std::string m_User;
    std::string m_Token;

    AFDian(std::string user, std::string token) : m_User(std::move(user)), m_Token(std::move(token)) {};

    [[nodiscard]] nlohmann::json WebBody(const nlohmann::json& root) const;

    static std::string SendPostRequest(const nlohmann::json& body);
    
public:
    static bool Auth();

};


#endif //AUTH_AFDIAN_H
