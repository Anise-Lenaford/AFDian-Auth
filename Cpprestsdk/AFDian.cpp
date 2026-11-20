#include "AFDian.h"
#include <filesystem>
#include "openssl/evp.h"

web::json::value AFDian::WebBody(web::json::value root) const
{
    using namespace std;
    using namespace web::json;
    using namespace utility::conversions;

    wstring timeStamp = to_wstring(time(nullptr));
    wstring jsonString = root.serialize();
    wstring aSige = Hash(m_Token + L"params" + jsonString + L"ts" + timeStamp + L"user_id" + m_User);
    if (aSige.empty()) return {};

    value aPacket;
    aPacket[U("user_id")] = value::string(m_User);
    aPacket[U("params")] = value::string(jsonString);
    aPacket[U("ts")] = value::string(timeStamp);
    aPacket[U("sign")] = value::string(aSige);

    return aPacket;

}

pplx::task<utility::string_t> AFDian::SendPostRequest(web::json::value body)
{
    web::http::client::http_client client(U("https://afdian.com"));

    web::http::http_request request(web::http::methods::POST);
    request.set_request_uri(U("/api/open/query-sponsor"));
    request.headers().add(U("User-Agent"), U("User"));
    request.headers().add(U("Content-Type"), U("application/json"));

    web::json::value request_body = body;
    request.set_body(request_body);

    return client.request(request).then([](web::http::http_response response) -> pplx::task<utility::string_t>
        {
            return response.extract_string();
        });

}


web::json::value AFDian::ReadJsonFile(const char* path)
{
    using namespace std;
    using namespace utility;
    using namespace web::json;

    if (!filesystem::exists(path)) return {};

    ifstream file(path);
    if (!file.is_open()) return {};

    stringstream buffer;
    buffer << file.rdbuf();
    string file_content = buffer.str();

    string_t json_content = conversions::to_string_t(file_content);
    value json_value = value::parse(json_content);

    return json_value;

}



std::wstring AFDian::Hash(const std::wstring& input)
{
    using namespace utility::conversions;

    std::string iString = to_utf8string(input);
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx)
    {
        if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) == 1 &&
            EVP_DigestUpdate(ctx, iString.data(), iString.size()) == 1 &&
            EVP_DigestFinal_ex(ctx, md, &md_len) == 1)
        {
            std::ostringstream oss;
            oss << std::hex << std::setfill('0');
            for (unsigned int i = 0; i < md_len; ++i)
            {
                oss << std::setw(2) << static_cast<unsigned int>(md[i]);
            }
            EVP_MD_CTX_free(ctx);
            return to_utf16string(oss.str());
        }
        EVP_MD_CTX_free(ctx);
    }
    return L"";
}



bool AFDian::Auth()
{
    using namespace web::json;

    value config = AFDian::ReadJsonFile(("_.config"));
    if (config.is_null()) return false;

    value launcher = config[(U("_"))];
    std::wstring user = launcher[(U("User"))].as_string();
    std::wstring token = launcher[(U("Token"))].as_string();

    AFDian aUser(user, token);

    value uAuthBody;
    uAuthBody[(U("user_id"))] = value::string(aUser.m_User);
    value cBody = aUser.WebBody(uAuthBody);

    std::wstring uResponse = aUser.SendPostRequest(cBody).get();
    value userJson = value::parse(uResponse);
    if (userJson[(U("ec"))].as_integer() != 200) return false;

    AFDian oUser((L"_"), (L"_"));

    value pageBody;
    pageBody[(U("page"))] = 1;
    value oBody = oUser.WebBody(pageBody);

    std::wstring pResponse = oUser.SendPostRequest(oBody).get();
    value pJson = value::parse(pResponse);
    if (pJson[(U("ec"))].as_integer() != 200) return false;

    value dataRoot = pJson[(U("data"))];
    int total_page = dataRoot[(U("total_page"))].as_integer();

    for (int page = 1; page <= total_page; page++)
    {
        value cPage;
        cPage[(U("page"))] = page;
        value cBody = oUser.WebBody(cPage);

        std::wstring uPage = oUser.SendPostRequest(cBody).get();
        value pageJson = value::parse(uPage);
        if (pageJson[(U("ec"))].as_integer() != 200) continue;

        const array& listRoot = pageJson.at((U("data"))).at((U("list"))).as_array();

        for (const value& rList : listRoot)
        {
            value userRoot = rList.at((U("user")));
            std::wstring uAuth = userRoot[(U("user_id"))].as_string();
            if (user != uAuth) continue;

            const array& planRoot = rList.at((U("sponsor_plans"))).as_array();
            for (const value& rPlan : planRoot)
            {
                int expire_time = rPlan.at((U("expire_time"))).as_integer();

                std::wstring planName = rPlan.at((U("name"))).as_string();

                if (planName == (L"_") && expire_time >= time(nullptr))
                {
                    return true;
                }

            }
            return false;

        }
    }

    return false;

}

