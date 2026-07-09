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

  struct Variable {
    std::string id;
    std::string name;
    std::string type;
    std::string default_value;
  };

  struct Function {
    std::string id;
    std::string name;
    std::vector<std::pair<std::string, std::string>> inputs;   // types:name
    std::vector<std::pair<std::string, std::string>> outputs;  // types:name
  };

  struct PinFormatInfo {
    std::string name;
    std::string color;
  };

  struct DrawerSession {
    std::vector<Variable> vars;
    std::vector<Function> functions;
    std::string selected_var;
    std::string selected_function;
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

  std::vector<Variable> load_variables_from_file(const std::string &storage_path);

}  // namespace TestCPP
#endif  // TESTCPP_HELPERS