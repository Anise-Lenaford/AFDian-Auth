#pragma once
#include <cpprest/http_client.h>
#include <cpprest/json.h>

class AFDian
{
public:
	std::wstring m_User;
	std::wstring m_Token;

	AFDian(std::wstring user, std::wstring token) : m_User(user), m_Token(token) {};

	web::json::value WebBody(web::json::value root) const;
	pplx::task<utility::string_t> SendPostRequest(web::json::value body);

	static std::wstring Hash(const std::wstring& input);
	static web::json::value ReadJsonFile(const char* path);

	static bool Auth();

};

