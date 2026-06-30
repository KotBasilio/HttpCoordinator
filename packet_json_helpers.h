#pragma once

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

#include "json.hpp"

namespace Sample::UI::Controllers {

using Json = nlohmann::json;
using JsonPointer = nlohmann::json::json_pointer;

const Json* NodeAt(const Json& j, const JsonPointer& ptr) noexcept;
std::string GetStr(const Json& obj, const char* key, std::string_view fallback = {});
std::string StrAt(const Json& j, const JsonPointer& ptr, std::string_view fallback = {});
std::optional<std::string> StringAt(const Json& j, const JsonPointer& ptr);
bool BoolAt(const Json& j, const JsonPointer& ptr, bool fallback = false) noexcept;
double MonotonicNowSeconds();

std::string ExtractUserIdentity(const Json& p);
std::string ExtractHydraKernelSessionId(const Json& p);
std::string ExtractSessionId(const Json& p);
std::string ExtractNicknameFromStaticData(const Json& memberData);
std::string ExtractFactsHeaderValue(const Json& p, const char* key);
std::string JsonGetString(const Json& j, std::initializer_list<const char*> path);

} // namespace Sample::UI::Controllers
