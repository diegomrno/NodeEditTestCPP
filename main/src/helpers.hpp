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

  struct FunctionPin {
    std::string name;
    std::string type;
    std::string default_value;
  };

  inline bool operator==(const FunctionPin &a, const FunctionPin &b) {
    return a.name == b.name && a.type == b.type && a.default_value == b.default_value;
  }

  struct Function {
    std::string id;
    std::string name;
    std::vector<FunctionPin> inputs;
    std::vector<FunctionPin> outputs;
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

  inline std::string DefaultLiteralValueForType(const std::string &type) {
    if (type == "bool")
      return "false";
    if (type == "int")
      return "0";
    if (type == "float")
      return "0.0";
    return "";
  }

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

  inline void to_json(nlohmann::json &j, const FunctionPin &p) {
    j = nlohmann::json{
      { "name", p.name },
      { "type", p.type },
      { "default_value", p.default_value },
    };
  }

  inline void from_json(const nlohmann::json &j, FunctionPin &p) {
    p.name = j.value("name", std::string());
    p.type = j.value("type", std::string("bool"));
    p.default_value = j.value("default_value", std::string());
  }

  inline void to_json(nlohmann::json &j, const Function &f) {
    j = nlohmann::json{
      { "id", f.id },
      { "name", f.name },
      { "inputs", f.inputs },
      { "outputs", f.outputs },
    };
  }

  inline void from_json(const nlohmann::json &j, Function &f) {
    f.id = j.value("id", std::string());
    f.name = j.value("name", std::string());
    f.inputs = j.value("inputs", std::vector<FunctionPin>());
    f.outputs = j.value("outputs", std::vector<FunctionPin>());
  }

  std::vector<Function> load_functions_from_file(const std::string &storage_path);
  std::vector<Variable> load_variables_from_file(const std::string &storage_path);

}  // namespace TestCPP
#endif  // TESTCPP_HELPERS