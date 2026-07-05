#include "transpiler.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "module.hpp"

namespace TestCPP {

  namespace {

    ReturnValues CallIe(const char *action, const std::string &json) {
      auto args = ArgumentValues(json);
      auto ret = ReturnValues();
      vxe::call_input_event("infinitehq.nodeedit", action, args, ret);
      return ret;
    }

    bool StartsWithStr(const std::string &s, const std::string &prefix) {
      return s.rfind(prefix, 0) == 0;
    }

    struct TranspileCtx {
      std::string session_id;
      std::unordered_map<std::string, TestCPP::Variable> vars_by_id;  // var.id : variable
      std::unordered_map<std::string, std::string> var_cpp_name;      // var.id : cpp identifier
      std::unordered_set<std::string> visiting;                       // cycle guard for the current flow chain
      bool had_error = false;
    };

    std::string GetNodeTypeId(TranspileCtx &ctx, const std::string &node_id) {
      nlohmann::json j;
      j["session_id"] = ctx.session_id;
      j["node_id"] = node_id;
      auto ret = CallIe("get_node_type_id", j.dump());
      return ret.get_json().value("type_id", "");
    }

    nlohmann::json GetNodeData(TranspileCtx &ctx, const std::string &node_id) {
      nlohmann::json j;
      j["session_id"] = ctx.session_id;
      j["node_id"] = node_id;
      auto ret = CallIe("get_node_data", j.dump());
      const auto &rj = ret.get_json();
      return rj.contains("datas") ? rj["datas"] : nlohmann::json::object();
    }

    std::string FindNodeBySchemaId(TranspileCtx &ctx, const std::string &schema_id) {
      nlohmann::json j;
      j["session_id"] = ctx.session_id;
      j["schema_id"] = schema_id;
      auto ret = CallIe("find_node_by_schema_id", j.dump());
      return ret.get_json().value("node_id", "");
    }

    std::vector<std::string> GetNextNodes(TranspileCtx &ctx, const std::string &node_id, const std::string &output_id) {
      nlohmann::json j;
      j["session_id"] = ctx.session_id;
      j["node_id"] = node_id;
      j["output_id"] = output_id;
      auto ret = CallIe("get_next_nodes", j.dump());
      std::vector<std::string> out;
      const auto &rj = ret.get_json();
      if (rj.contains("node_ids") && rj["node_ids"].is_array())
        for (const auto &v : rj["node_ids"])
          out.push_back(v.get<std::string>());
      return out;
    }

    std::vector<std::string> GetPreviousNodes(TranspileCtx &ctx, const std::string &node_id, const std::string &input_id) {
      nlohmann::json j;
      j["session_id"] = ctx.session_id;
      j["node_id"] = node_id;
      j["input_id"] = input_id;
      auto ret = CallIe("get_previous_nodes", j.dump());
      std::vector<std::string> out;
      const auto &rj = ret.get_json();
      if (rj.contains("node_ids") && rj["node_ids"].is_array())
        for (const auto &v : rj["node_ids"])
          out.push_back(v.get<std::string>());
      return out;
    }

    std::string CppTypeForPinType(const std::string &pin_type) {
      if (pin_type == "bool")
        return "bool";
      if (pin_type == "int")
        return "int";
      if (pin_type == "float")
        return "float";
      if (pin_type == "string")
        return "std::string";
      // TODO default "auto" fallback usage, use better ?
      return "auto";
    }

    std::string EscapeCppString(const std::string &s) {
      std::string out;
      out.reserve(s.size());
      for (char c : s) {
        switch (c) {
          case '"': out += "\\\""; break;
          case '\\': out += "\\\\"; break;
          case '\n': out += "\\n"; break;
          case '\t': out += "\\t"; break;
          default: out += c;
        }
      }
      return out;
    }

    std::string LiteralFromData(const nlohmann::json &datas, const std::string &pin_id, const std::string &pin_type) {
      if (pin_type == "bool")
        return datas.value(pin_id, false) ? "true" : "false";
      if (pin_type == "int")
        return std::to_string(datas.value(pin_id, 0));
      if (pin_type == "float")
        return std::to_string(datas.value(pin_id, 0.0f)) + "f";
      if (pin_type == "string")
        return "\"" + EscapeCppString(datas.value(pin_id, std::string())) + "\"";
      return "{}";
    }

    // turns variables into valid cpp identifiers
    std::string SanitizeIdentifier(const std::string &raw) {
      std::string out;
      out.reserve(raw.size());
      for (char c : raw)
        out += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
      if (out.empty() || std::isdigit(static_cast<unsigned char>(out[0])))
        out = "_" + out;
      return out;
    }

    std::string CppVarName(TranspileCtx &ctx, const std::string &var_id) {
      auto it = ctx.var_cpp_name.find(var_id);
      if (it != ctx.var_cpp_name.end())
        return it->second;
      std::string name = "var_" + SanitizeIdentifier(var_id);
      ctx.var_cpp_name[var_id] = name;
      return name;
    }

    std::string Indent(int depth) {
      return std::string(static_cast<size_t>(depth) * 2, ' ');
    }

    // Graph traversal
    std::string TranspileExpression(TranspileCtx &ctx, const std::string &node_id, const std::string &expected_type);

    std::string ResolveInput(
        TranspileCtx &ctx,
        const std::string &node_id,
        const std::string &pin_id,
        const std::string &pin_type,
        const nlohmann::json &node_datas) {
      auto producers = GetPreviousNodes(ctx, node_id, pin_id);
      if (producers.size() > 1)
        ctx.had_error = true;
      if (!producers.empty())
        return TranspileExpression(ctx, producers.front(), pin_type);
      return LiteralFromData(node_datas, pin_id, pin_type);
    }

    std::string TranspileExpression(TranspileCtx &ctx, const std::string &node_id, const std::string &expected_type) {
      std::string type_id = GetNodeTypeId(ctx, node_id);
      if (type_id.empty()) {
        ctx.had_error = true;
        return "/* unresolved node " + node_id + " */ " + LiteralFromData(nlohmann::json::object(), "", expected_type);
      }

      if (StartsWithStr(type_id, "varget_")) {
        std::string var_id = type_id.substr(std::string("varget_").size());
        if (ctx.vars_by_id.find(var_id) == ctx.vars_by_id.end()) {
          ctx.had_error = true;
          return "/* TODO: unknown variable '" + var_id + "' */ " +
                 LiteralFromData(nlohmann::json::object(), "", expected_type);
        }
        return CppVarName(ctx, var_id);
      }

      if (type_id == "is_int_higher_than_int") {
        auto datas = GetNodeData(ctx, node_id);
        std::string lhs = ResolveInput(ctx, node_id, "int1", "int", datas);
        std::string rhs = ResolveInput(ctx, node_id, "int2", "int", datas);
        return "(" + lhs + " > " + rhs + ")";
      }

      ctx.had_error = true;
      return "/* TODO: unsupported expression node type '" + type_id + "' */ " +
             LiteralFromData(nlohmann::json::object(), "", expected_type);
    }

    void TranspileFlow(TranspileCtx &ctx, const std::string &node_id, int depth, std::string &out) {
      if (node_id.empty())
        return;

      if (!ctx.visiting.insert(node_id).second) {
        out += Indent(depth) + "// cycle detected at node " + node_id + ", stopping this path here\n";
        return;
      }

      std::string type_id = GetNodeTypeId(ctx, node_id);

      if (type_id == "console_output") {
        auto datas = GetNodeData(ctx, node_id);
        std::string msg = ResolveInput(ctx, node_id, "string", "string", datas);
        out += Indent(depth) + "std::cout << " + msg + " << std::endl;\n";
        for (const auto &next : GetNextNodes(ctx, node_id, "output_flow"))
          TranspileFlow(ctx, next, depth, out);

      } else if (type_id == "sleep") {
        auto datas = GetNodeData(ctx, node_id);
        std::string seconds = ResolveInput(ctx, node_id, "int", "int", datas);
        out += Indent(depth) + "std::this_thread::sleep_for(std::chrono::seconds(" + seconds + "));\n";
        for (const auto &next : GetNextNodes(ctx, node_id, "output_flow"))
          TranspileFlow(ctx, next, depth, out);

      } else if (type_id == "branch") {
        auto datas = GetNodeData(ctx, node_id);
        std::string cond = ResolveInput(ctx, node_id, "bool", "bool", datas);
        out += Indent(depth) + "if (" + cond + ") {\n";
        for (const auto &next : GetNextNodes(ctx, node_id, "output_true"))
          TranspileFlow(ctx, next, depth + 1, out);
        out += Indent(depth) + "} else {\n";
        for (const auto &next : GetNextNodes(ctx, node_id, "output_false"))
          TranspileFlow(ctx, next, depth + 1, out);
        out += Indent(depth) + "}\n";

      } else if (StartsWithStr(type_id, "varset_")) {
        std::string var_id = type_id.substr(std::string("varset_").size());
        auto it = ctx.vars_by_id.find(var_id);
        if (it == ctx.vars_by_id.end()) {
          ctx.had_error = true;
          out += Indent(depth) + "// TODO: unknown variable '" + var_id + "' referenced by node " + node_id + "\n";
        } else {
          auto datas = GetNodeData(ctx, node_id);
          std::string value_expr = ResolveInput(ctx, node_id, "value", it->second.type, datas);
          out += Indent(depth) + CppVarName(ctx, var_id) + " = " + value_expr + ";\n";
        }
        for (const auto &next : GetNextNodes(ctx, node_id, "output_flow"))
          TranspileFlow(ctx, next, depth, out);

      } else if (type_id == "on_setup" || type_id == "on_loop") {
        for (const auto &next : GetNextNodes(ctx, node_id, "output_flow"))
          TranspileFlow(ctx, next, depth, out);

      } else {
        ctx.had_error = true;
        out += Indent(depth) + "// TODO: unsupported flow node type '" + type_id + "' (node " + node_id + ")\n";
      }

      ctx.visiting.erase(node_id);
    }

  }  // namespace

  bool transpile_graph(const std::string &session_id, const std::string &storage_path, std::string &out_cpp_path) {
    if (session_id.empty()) {
      get_current_context()->interface->log_error("[TestCPP] Cannot transpile: no active graph session.");
      return false;
    }

    TranspileCtx ctx;
    ctx.session_id = session_id;

    for (auto &v : TestCPP::load_variables_from_file(storage_path))
      ctx.vars_by_id[v.id] = v;

    std::string on_setup_id = FindNodeBySchemaId(ctx, "on_setup");
    std::string on_loop_id = FindNodeBySchemaId(ctx, "on_loop");

    if (on_setup_id.empty()) {
      get_current_context()->interface->log_error("[TestCPP] Transpile failed: no 'on_setup' node found in the graph.");
      return false;
    }
    if (on_loop_id.empty()) {
      get_current_context()->interface->log_error("[TestCPP] Transpile failed: no 'on_loop' node found in the graph.");
      return false;
    }

    std::string var_decls;
    for (auto &[var_id, var] : ctx.vars_by_id) {
      std::string cpp_type = CppTypeForPinType(var.type);
      std::string default_literal;

      if (var.type == "string") {
        default_literal = "\"" + EscapeCppString(var.default_value) + "\"";
      } else if (var.type == "bool") {
        default_literal = (var.default_value == "true" || var.default_value == "1") ? "true" : "false";
      } else if (var.default_value.empty()) {
        default_literal = (var.type == "float") ? "0.0f" : "0";
      } else {
        default_literal = var.default_value + (var.type == "float" ? "f" : "");
      }

      var_decls += cpp_type + " " + CppVarName(ctx, var_id) + " = " + default_literal + "; // " + var.name + "\n";
    }

    std::string setup_body;
    for (const auto &next : GetNextNodes(ctx, on_setup_id, "output_flow"))
      TranspileFlow(ctx, next, 1, setup_body);

    std::string loop_body;
    for (const auto &next : GetNextNodes(ctx, on_loop_id, "output_flow"))
      TranspileFlow(ctx, next, 1, loop_body);

    std::ostringstream file;
    file << "// Auto-generated by the Vortex module infinitehq.testcpp \n";
    file << "// re-running the transpiler will overwrite this file.\n";
    file << "// NEVER STOP HACKING ! \n";
    file << "#include <iostream>\n";
    file << "#include <string>\n";
    file << "#include <thread>\n";
    file << "#include <chrono>\n\n";

    file << "// Variables \n";
    file << var_decls << "\n";

    file << "void OnSetup();\n";
    file << "void OnLoop();\n\n";

    file << "int main() {\n";
    file << "  OnSetup();\n";
    file << "  while (true) {\n";
    file << "    OnLoop();\n";
    file << "  }\n";
    file << "  return 0;\n";
    file << "}\n\n";

    file << "void OnSetup() {\n" << setup_body << "}\n\n";
    file << "void OnLoop() {\n" << loop_body << "}\n";

    std::filesystem::path storage_p(storage_path);
    std::filesystem::path out_p = storage_p.has_parent_path() ? storage_p.parent_path() / "generated_program.cpp"
                                                              : std::filesystem::path("generated_program.cpp");

    try {
      if (out_p.has_parent_path())
        std::filesystem::create_directories(out_p.parent_path());

      std::ofstream ofs(out_p);
      if (!ofs.is_open()) {
        get_current_context()->interface->log_error("[TestCPP] Could not open '" + out_p.string() + "' for writing.");
        return false;
      }
      ofs << file.str();
    } catch (const std::exception &e) {
      get_current_context()->interface->log_error(std::string("[TestCPP] Failed to write generated file: ") + e.what());
      return false;
    }

    out_cpp_path = out_p.string();

    if (ctx.had_error) {
      get_current_context()->interface->log_error(
          "[TestCPP] Transpile completed with warnings — check the TODO comments in " + out_cpp_path);
    }

    return true;
  }

  // Reminder, this is only for testing, more capabilities will comming with new official vortex modules
  bool compile_and_run(const std::string &cpp_path) {
    std::filesystem::path p(cpp_path);
    std::filesystem::path exe_path = p.parent_path() / p.stem();

#if defined(_WIN32)
    exe_path += ".exe";
    std::string compile_cmd = "g++ -std=c++17 \"" + p.string() + "\" -o \"" + exe_path.string() + "\"";
#else
    std::string compile_cmd = "c++ -std=c++17 \"" + p.string() + "\" -o \"" + exe_path.string() + "\"";
#endif

    int compile_result = std::system(compile_cmd.c_str());
    if (compile_result != 0) {
      get_current_context()->interface->log_error("[TestCPP] Compilation failed (see console output above).");
      return false;
    }

#if defined(_WIN32)
    std::string run_cmd = "start \"\" \"" + exe_path.string() + "\"";
#else
    std::string run_cmd = "\"" + exe_path.string() + "\" &";
#endif

    std::system(run_cmd.c_str());
    return true;
  }

}  // namespace TestCPP