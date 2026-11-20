
#include <chrono>
#include <ranges>
#include <algorithm>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "md5.h"
#include "AFDian.h"
#include "File.h"

nlohmann::json AFDian::WebBody(const nlohmann::json& root) const {
    using json = nlohmann::json;

    auto ts = std::to_string(std::time(nullptr));
    auto jsonString = root.dump();

    std::string signStr = m_Token + "params" + jsonString + "ts" + ts + "user_id" + m_User;

    MD5 md5;
    auto sign = md5(signStr);
    if (sign.empty()) return {};

    json packet;
    packet["user_id"] = m_User;
    packet["params"] = jsonString;
    packet["ts"] = ts;
    packet["sign"] = sign;
    return packet;
}

std::string AFDian::SendPostRequest(const nlohmann::json &body) {
    httplib::Client client("https://afdian.com");
    client.set_connection_timeout(4, 0);
    client.set_read_timeout(8, 0);

    client.enable_server_certificate_verification(false);

    std::string bodyStr = body.dump();
    auto res = client.Post("/api/open/query-sponsor", bodyStr, "application/json");

    if (res && res->status == 200) {
        return res->body;
    }
    return {};
}

bool AFDian::Auth() {
    using json = nlohmann::json;

    std::string conString = File::ReadString("_.config");
    json config = json::parse(conString);

    if (config.is_null()) return false;
    if (!config.contains("_")) return false;

    json launcher = config["_"];
    std::string user = launcher.value("User", "");
    std::string token = launcher.value("Token", "");
    if (user.empty() || token.empty()) return false;

    AFDian aUser(user, token);

    json uAuthBody;
    uAuthBody["user_id"] = aUser.m_User;
    json cBody = aUser.WebBody(uAuthBody);

    std::string uResponse = AFDian::SendPostRequest(cBody);
    if (uResponse.empty()) return false;

    json userJson = json::parse(uResponse);
    if (!userJson.contains("ec") || userJson["ec"].get<int>() != 200)
        return false;

    AFDian oUser(("_"), ("_"));

    json pageBody;
    pageBody["page"] = 1;
    json oBody = oUser.WebBody(pageBody);
    std::string pResponse = AFDian::SendPostRequest(oBody);
    if (pResponse.empty()) return false;

    json pJson = json::parse(pResponse);
    if (!pJson.contains("ec") || pJson["ec"].get<int>() != 200)
        return false;

    json dataRoot = pJson["data"];
    int total_page = dataRoot.value("total_page", 0);

    for (int page = 1; page <= total_page; ++page) {
        json cPage;
        cPage["page"] = page;
        json user_body = oUser.WebBody(cPage);
        std::string uPage = AFDian::SendPostRequest(user_body);
        if (uPage.empty()) continue;

        json pageJson = json::parse(uPage);
        if (!pageJson.contains("ec") || pageJson["ec"].get<int>() != 200)
            continue;

        if (!pageJson.contains("data") || !pageJson["data"].contains("list"))
            continue;
        auto& listRoot = pageJson["data"]["list"];
        if (!listRoot.is_array()) continue;

        for (auto& rList : listRoot) {
            if (!rList.contains("user")) continue;
            std::string uAuth = rList["user"].value("user_id", "");
            if (user != uAuth) continue;

            if (!rList.contains("sponsor_plans") || !rList["sponsor_plans"].is_array())
                return false;

            auto now = std::time(nullptr);
            const auto& plans = rList["sponsor_plans"];
            bool vPlan = std::ranges::any_of(plans, [now](const auto& rPlan) {
                int expire_time = rPlan.value("expire_time", 0);
                std::string planName = rPlan.value("name", "");
                return planName == "_" && expire_time >= now;
            });
            return vPlan;
        }
    }
    return false;

}