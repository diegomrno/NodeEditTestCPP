#pragma once
#include <vxcore/include/vortex.h>
#include <vxcore/include/vortex_internals.h>

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <vxgui/editor/main/editor.hpp>
#ifndef TESTCPP_HELPERS
#define TESTCPP_HELPERS
namespace TestCPP {

  // Only name / type / default_value are ever persisted to disk. `id` is kept
  // so the corresponding varget_/varset_ schema ids stay stable across saves.
  struct Variable {
    std::string id;
    std::string name;
    std::string type;
    std::string default_value;
  };

  struct PinFormatInfo {
    std::string name;
    std::string color;
  };

  struct DrawerSession {
    std::vector<Variable> vars;
    std::string selected_var;
    std::unordered_map<std::string, PinFormatInfo> pin_format_cache;
  };

  inline void to_json(nlohmann::json &j, const Variable &v) {
    j = nlohmann::json{
      { "id", v.id },
      { "name", v.name },
      { "type", v.type },
      { "default_value", v.default_value },
    };
  }

  inline void from_json(const nlohmann::json &j, Variable &v) {
    v.id = j.value("id", std::string());
    v.name = j.value("name", std::string());
    v.type = j.value("type", std::string("bool"));
    v.default_value = j.value("default_value", std::string());
  }

}  // namespace TestCPP
#endif  // TESTCPP_HELPERS