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
  inline void to_json(nlohmann::json &j, const Function &f) {
    nlohmann::json in = nlohmann::json::array();
    for (const auto &[t, n] : f.inputs)
      in.push_back({ { "type", t }, { "name", n } });

    nlohmann::json out = nlohmann::json::array();
    for (const auto &[t, n] : f.outputs)
      out.push_back({ { "type", t }, { "name", n } });

    j = nlohmann::json{
      { "id", f.id },
      { "name", f.name },
      { "inputs", in },
      { "outputs", out },
    };
  }

  inline void from_json(const nlohmann::json &j, Function &f) {
    f.id = j.value("id", std::string());
    f.name = j.value("name", std::string());

    f.inputs.clear();
    if (j.contains("inputs") && j["inputs"].is_array())
      for (const auto &pj : j["inputs"])
        f.inputs.emplace_back(pj.value("type", std::string()), pj.value("name", std::string()));

    f.outputs.clear();
    if (j.contains("outputs") && j["outputs"].is_array())
      for (const auto &pj : j["outputs"])
        f.outputs.emplace_back(pj.value("type", std::string()), pj.value("name", std::string()));
  }

  std::vector<Function> load_functions_from_file(const std::string &storage_path);
  std::vector<Variable> load_variables_from_file(const std::string &storage_path);

}  // namespace TestCPP
#endif  // TESTCPP_HELPERS