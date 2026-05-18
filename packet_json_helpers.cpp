#include "packet_json_helpers.h"

#include <utility>

#pragma message("packet_json_helpers.cpp REV: rich kv v0.1")

namespace Sample::UI::Controllers
{

using namespace nlohmann::literals; // enables "... "_json_pointer

const Json* NodeAt(const Json& j, const JsonPointer& ptr) noexcept
{
   // contains(ptr) is a safe existence check for pointer paths
   // (no insertion, no out_of_range).
   if (!j.contains(ptr)) return nullptr;
   return &j.at(ptr);
}

std::string GetStr(const Json& obj, const char* key, std::string_view fallback)
{
   auto it = obj.find(key);
   if (it == obj.end() || it->is_null())
      return std::string(fallback);

   if (it->is_string())          return it->get<std::string>();
   if (it->is_number_integer())  return std::to_string(it->get<long long>());
   if (it->is_number_unsigned()) return std::to_string(it->get<unsigned long long>());
   if (it->is_number_float())    return std::to_string(it->get<double>());
   if (it->is_boolean())         return it->get<bool>() ? "true" : "false";

   return std::string(fallback);
}

std::string StrAt(const Json& j, const JsonPointer& ptr, std::string_view fallback)
{
   auto* n = NodeAt(j, ptr);
   if (!n || n->is_null()) return std::string(fallback);

   if (n->is_string())          return n->get<std::string>();
   if (n->is_number_integer())  return std::to_string(n->get<long long>());
   if (n->is_number_unsigned()) return std::to_string(n->get<unsigned long long>());
   if (n->is_number_float())    return std::to_string(n->get<double>());
   if (n->is_boolean())         return n->get<bool>() ? "true" : "false";

   return std::string(fallback);
}

std::optional<std::string> StringAt(const Json& j, const JsonPointer& ptr)
{
   if (auto* n = NodeAt(j, ptr); n && n->is_string())
      return n->get<std::string>();
   return std::nullopt;
}

bool BoolAt(const Json& j, const JsonPointer& ptr, bool fallback) noexcept
{
   if (auto* n = NodeAt(j, ptr); n && n->is_boolean())
      return n->get<bool>();
   return fallback;
}

std::string ExtractUserIdentity(const Json& p)
{
   static const auto P1 = "/data/userContext/data/userIdentity"_json_pointer;
   static const auto P2 = "/userContext/data/userIdentity"_json_pointer;
   static const auto P3 = "/context/data/userIdentity"_json_pointer;

   if (auto s = StringAt(p, P1)) return std::move(*s);
   if (auto s = StringAt(p, P2)) return std::move(*s);
   if (auto s = StringAt(p, P3)) return std::move(*s);
   return {};
}

std::string ExtractHydraKernelSessionId(const Json& p)
{
   static const auto P1 = "/data/userContext/data/kernelSessionId"_json_pointer;
   static const auto P2 = "/userContext/data/kernelSessionId"_json_pointer;
   static const auto P3 = "/context/data/kernelSessionId"_json_pointer;

   if (auto s = StringAt(p, P1)) return std::move(*s);
   if (auto s = StringAt(p, P2)) return std::move(*s);
   if (auto s = StringAt(p, P3)) return std::move(*s);
   return {};
}

std::string ExtractSessionId(const Json& p)
{
   // PresenceSessionUpdate carries: id.id.
   static const auto PID = "/id/id"_json_pointer;
   return StrAt(p, PID, "");
}

std::string ExtractNicknameFromStaticData(const Json& memberData)
{
   // memberData.staticData is usually a JSON string; best effort parse.
   std::string sd = GetStr(memberData, "staticData", "");
   if (sd.empty()) return "";

   // Sometimes it is already cut or not JSON; ignore quietly.
   if (sd.size() < 2 || sd.front() != '{') return "";

   auto j = Json::parse(sd, nullptr, false);
   if (!j.is_discarded()) {
      auto it = j.find("nickname");
      if (it != j.end() && it->is_string())
         return it->get<std::string>();
   }
   return "";
}

std::string ExtractFactsHeaderValue(const Json& p, const char* key)
{
   static const auto PCTX = "/header/context"_json_pointer;
   const Json* ctx = NodeAt(p, PCTX);
   if (!ctx || !ctx->is_array()) return {};

   for (const auto& e : *ctx) {
      if (!e.is_object()) continue;
      const std::string name = GetStr(e, "propertyName", "");
      if (name == key) {
         return GetStr(e, "propertyValue", "");
      }
   }
   return {};
}

std::string JsonGetString(const Json& j, std::initializer_list<const char*> path)
{
   const Json* cur = &j;
   for (const char* key : path) {
      auto it = cur->find(key);
      if (it == cur->end()) return {};
      cur = &(*it);
   }
   if (cur->is_string())          return cur->get<std::string>();
   if (cur->is_number_integer())  return std::to_string(cur->get<long long>());
   if (cur->is_number_unsigned()) return std::to_string(cur->get<unsigned long long>());
   if (cur->is_number_float())    return std::to_string(cur->get<double>());
   if (cur->is_boolean())         return cur->get<bool>() ? "true" : "false";
   return {};
}

} // namespace Sample::UI::Controllers
