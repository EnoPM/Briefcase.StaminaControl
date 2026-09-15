#pragma once
#include <nlohmann/json.hpp>
inline nlohmann::json config_defaults(const nlohmann::json& schema) {
 auto result=nlohmann::json::object();
 for(auto& [key,field]:schema.at("properties").items()) if(field.contains("default")) result[key]=field.at("default");
 return result;
}
