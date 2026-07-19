// transpiler.cpp

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

    std::string Join(const std::vector<std::string> &parts, const std::string &sep) {
      std::string out;
      for (size_t i = 0; i < parts.size(); ++i) {
        if (i)
          out += sep;
        out += parts[i];
      }
      return out;
    }

    std::string SanitizeIdentifier(const std::string &raw) {
      std::string out;
      out.reserve(raw.size());
      for (char c : raw)
        out += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
      if (out.empty() || std::isdigit(static_cast<unsigned char>(out[0])))
        out = "_" + out;
      return out;
    }

    std::string CppParamName(const std::string &raw_name) {
      return "p_" + SanitizeIdentifier(raw_name);
    }

    struct GraphHandle {
      bool silent = false;
      std::string ref;
    };

    nlohmann::json
    CallGraphEvent(const GraphHandle &h, const char *live_action, const char *silent_action, nlohmann::json j) {
      if (h.silent) {
        j["path"] = h.ref;
        return CallIe(silent_action, j.dump()).get_json();
      }
      j["session_id"] = h.ref;
      return CallIe(live_action, j.dump()).get_json();
    }

    struct FunctionInfo {
      TestCPP::Function decl;
      std::string folder_path;
      std::string cpp_name;
    };

    struct TranspileCtx {
      GraphHandle graph;

      std::unordered_map<std::string, TestCPP::Variable> vars_by_id;
      std::unordered_map<std::string, std::string> var_cpp_name;

      std::unordered_map<std::string, FunctionInfo> functions_by_id;

      std::unordered_set<std::string> visiting;
      std::unordered_map<std::string, std::unordered_map<std::string, std::string>> call_result_vars;
      const FunctionInfo *current_function = nullptr;
      std::string current_function_start_schema;
      std::string current_function_end_schema;

      bool had_error = false;
    };

    std::string GetNodeTypeId(TranspileCtx &ctx, const std::string &node_id) {
      auto rj = CallGraphEvent(ctx.graph, "get_node_type_id", "get_node_type_id_silently", { { "node_id", node_id } });
      return rj.value("type_id", "");
    }

    nlohmann::json GetNodeData(TranspileCtx &ctx, const std::string &node_id) {
      auto rj = CallGraphEvent(ctx.graph, "get_node_data", "get_node_data_silently", { { "node_id", node_id } });
      return rj.contains("datas") ? rj["datas"] : nlohmann::json::object();
    }

    std::string FindNodeBySchemaId(TranspileCtx &ctx, const std::string &schema_id) {
      auto rj = CallGraphEvent(
          ctx.graph, "find_node_by_schema_id", "find_node_by_schema_id_silently", { { "schema_id", schema_id } });
      return rj.value("node_id", "");
    }

    std::vector<std::string> GetNextNodes(TranspileCtx &ctx, const std::string &node_id, const std::string &output_id) {
      auto rj = CallGraphEvent(
          ctx.graph, "get_next_nodes", "get_next_nodes_silently", { { "node_id", node_id }, { "output_id", output_id } });
      std::vector<std::string> out;
      if (rj.contains("node_ids") && rj["node_ids"].is_array())
        for (const auto &v : rj["node_ids"])
          out.push_back(v.get<std::string>());
      return out;
    }

    std::vector<std::string> GetPreviousNodes(TranspileCtx &ctx, const std::string &node_id, const std::string &input_id) {
      auto rj = CallGraphEvent(
          ctx.graph,
          "get_previous_nodes",
          "get_previous_nodes_silently",
          { { "node_id", node_id }, { "input_id", input_id } });
      std::vector<std::string> out;
      if (rj.contains("node_ids") && rj["node_ids"].is_array())
        for (const auto &v : rj["node_ids"])
          out.push_back(v.get<std::string>());
      return out;
    }

    std::string GetSourcePin(TranspileCtx &ctx, const std::string &node_id, const std::string &input_id) {
      auto rj = CallGraphEvent(
          ctx.graph,
          "get_connection_source_pin",
          "get_connection_source_pin_silently",
          { { "node_id", node_id }, { "input_id", input_id } });
      return rj.value("pin_id", "");
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

    std::string LiteralFromData(
        const nlohmann::json &datas,
        const std::string &pin_id,
        const std::string &pin_type,
        const std::string &fallback_default = "") {
      bool has_value = datas.contains(pin_id) && !datas[pin_id].is_null();

      if (pin_type == "bool") {
        if (has_value && datas[pin_id].is_boolean())
          return datas[pin_id].get<bool>() ? "true" : "false";
        if (!fallback_default.empty())
          return (fallback_default == "true" || fallback_default == "1") ? "true" : "false";
        return "false";
      }

      if (pin_type == "int") {
        if (has_value && datas[pin_id].is_number_integer())
          return std::to_string(datas[pin_id].get<int>());
        if (!fallback_default.empty()) {
          try {
            return std::to_string(std::stoi(fallback_default));
          } catch (...) {
          }
        }
        return "0";
      }

      if (pin_type == "float") {
        if (has_value && datas[pin_id].is_number())
          return std::to_string(datas[pin_id].get<float>()) + "f";
        if (!fallback_default.empty()) {
          try {
            return std::to_string(std::stof(fallback_default)) + "f";
          } catch (...) {
          }
        }
        return "0.0f";
      }

      if (pin_type == "string") {
        if (has_value && datas[pin_id].is_string())
          return "\"" + EscapeCppString(datas[pin_id].get<std::string>()) + "\"";
        return "\"" + EscapeCppString(fallback_default) + "\"";
      }

      return "{}";
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

    std::string TranspileExpression(
        TranspileCtx &ctx,
        const std::string &node_id,
        const std::string &source_pin,
        const std::string &expected_type);

    std::string ResolveInput(
        TranspileCtx &ctx,
        const std::string &node_id,
        const std::string &pin_id,
        const std::string &pin_type,
        const nlohmann::json &node_datas,
        const std::string &fallback_default = "") {
      auto producers = GetPreviousNodes(ctx, node_id, pin_id);
      if (producers.size() > 1)
        ctx.had_error = true;
      if (!producers.empty()) {
        std::string source_pin = GetSourcePin(ctx, node_id, pin_id);
        return TranspileExpression(ctx, producers.front(), source_pin, pin_type);
      }
      return LiteralFromData(node_datas, pin_id, pin_type, fallback_default);
    }
    std::string TranspileExpression(
        TranspileCtx &ctx,
        const std::string &node_id,
        const std::string &source_pin,
        const std::string &expected_type) {
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

      if (ctx.current_function && type_id == ctx.current_function_start_schema) {
        return CppParamName(source_pin);
      }

      if (ctx.functions_by_id.count(type_id)) {
        auto node_it = ctx.call_result_vars.find(node_id);
        if (node_it != ctx.call_result_vars.end()) {
          auto pin_it = node_it->second.find(source_pin);
          if (pin_it != node_it->second.end())
            return pin_it->second;
        }
        ctx.had_error = true;
        return "/* TODO: function call result not available for node " + node_id + " pin '" + source_pin + "' */ " +
               LiteralFromData(nlohmann::json::object(), "", expected_type);
      }

      if (type_id == "is_int_higher_than_int") {
        auto datas = GetNodeData(ctx, node_id);
        std::string lhs = ResolveInput(ctx, node_id, "int1", "int", datas);
        std::string rhs = ResolveInput(ctx, node_id, "int2", "int", datas);
        return "(" + lhs + " > " + rhs + ")";
      }

      if (type_id == "add_int" || type_id == "add_float") {
        auto datas = GetNodeData(ctx, node_id);
        std::string a = ResolveInput(ctx, node_id, "a", "", datas);
        std::string b = ResolveInput(ctx, node_id, "b", "", datas);
        return "(" + a + " + " + b + ")";
      }

      if (type_id == "subtract_int" || type_id == "subtract_float") {
        auto datas = GetNodeData(ctx, node_id);
        std::string a = ResolveInput(ctx, node_id, "a", "", datas);
        std::string b = ResolveInput(ctx, node_id, "b", "", datas);
        return "(" + a + " - " + b + ")";
      }

      if (type_id == "multiply_int" || type_id == "multiply_float") {
        auto datas = GetNodeData(ctx, node_id);
        std::string a = ResolveInput(ctx, node_id, "a", "", datas);
        std::string b = ResolveInput(ctx, node_id, "b", "", datas);
        return "(" + a + " * " + b + ")";
      }

      if (type_id == "divide_int" || type_id == "divide_float") {
        auto datas = GetNodeData(ctx, node_id);
        std::string a = ResolveInput(ctx, node_id, "a", "", datas);
        std::string b = ResolveInput(ctx, node_id, "b", "", datas);
        return "(" + a + " / " + b + ")";
      }

      if (type_id == "min_int" || type_id == "min_float") {
        auto datas = GetNodeData(ctx, node_id);
        std::string a = ResolveInput(ctx, node_id, "a", "", datas);
        std::string b = ResolveInput(ctx, node_id, "b", "", datas);
        return "std::min(" + a + ", " + b + ")";
      }

      if (type_id == "max_int" || type_id == "max_float") {
        auto datas = GetNodeData(ctx, node_id);
        std::string a = ResolveInput(ctx, node_id, "a", "", datas);
        std::string b = ResolveInput(ctx, node_id, "b", "", datas);
        return "std::max(" + a + ", " + b + ")";
      }

      if (type_id == "clamp_int" || type_id == "clamp_float") {
        auto datas = GetNodeData(ctx, node_id);
        std::string v = ResolveInput(ctx, node_id, "value", "", datas);
        std::string mn = ResolveInput(ctx, node_id, "min", "", datas);
        std::string mx = ResolveInput(ctx, node_id, "max", "", datas);
        return "std::clamp(" + v + ", " + mn + ", " + mx + ")";
      }

      if (type_id == "abs_int" || type_id == "abs_float") {
        auto datas = GetNodeData(ctx, node_id);
        std::string a = ResolveInput(ctx, node_id, "a", "", datas);
        return "std::abs(" + a + ")";
      }

      if (type_id == "round_float") {
        auto datas = GetNodeData(ctx, node_id);
        std::string a = ResolveInput(ctx, node_id, "a", "float", datas);
        return "static_cast<int>(std::round(" + a + "))";
      }

      if (type_id == "floor_float") {
        auto datas = GetNodeData(ctx, node_id);
        std::string a = ResolveInput(ctx, node_id, "a", "float", datas);
        return "static_cast<int>(std::floor(" + a + "))";
      }

      if (type_id == "ceil_float") {
        auto datas = GetNodeData(ctx, node_id);
        std::string a = ResolveInput(ctx, node_id, "a", "float", datas);
        return "static_cast<int>(std::ceil(" + a + "))";
      }

      if (type_id == "random_int") {
        auto datas = GetNodeData(ctx, node_id);
        std::string mn = ResolveInput(ctx, node_id, "min", "int", datas);
        std::string mx = ResolveInput(ctx, node_id, "max", "int", datas);
        return "(" + mn + " + (std::rand() % ((" + mx + ") - (" + mn + ") + 1)))";
      }

      if (type_id == "random_float") {
        auto datas = GetNodeData(ctx, node_id);
        std::string mn = ResolveInput(ctx, node_id, "min", "float", datas);
        std::string mx = ResolveInput(ctx, node_id, "max", "float", datas);
        return "(" + mn + " + static_cast<float>(std::rand()) / RAND_MAX * ((" + mx + ") - (" + mn + ")))";
      }

      if (type_id == "int_to_string" || type_id == "float_to_string") {
        auto datas = GetNodeData(ctx, node_id);
        std::string a = ResolveInput(ctx, node_id, "a", "", datas);
        return "std::to_string(" + a + ")";
      }

      if (type_id == "bool_to_string") {
        auto datas = GetNodeData(ctx, node_id);
        std::string a = ResolveInput(ctx, node_id, "a", "bool", datas);
        return "(std::string(" + a + " ? \"true\" : \"false\"))";
      }

      if (type_id == "string_to_int") {
        auto datas = GetNodeData(ctx, node_id);
        std::string a = ResolveInput(ctx, node_id, "a", "string", datas);
        return "std::stoi(" + a + ")";
      }

      if (type_id == "string_to_float") {
        auto datas = GetNodeData(ctx, node_id);
        std::string a = ResolveInput(ctx, node_id, "a", "string", datas);
        return "std::stof(" + a + ")";
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

      } else if (type_id == "close") {
        out += Indent(depth) + "TriggerClose();\n";
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

      } else if (ctx.current_function && type_id == ctx.current_function_end_schema) {
        const FunctionInfo &fn = *ctx.current_function;
        auto datas = GetNodeData(ctx, node_id);

        if (fn.decl.outputs.empty()) {
          out += Indent(depth) + "return;\n";
        } else if (fn.decl.outputs.size() == 1) {
          const auto &pin = fn.decl.outputs[0];
          std::string expr = ResolveInput(ctx, node_id, pin.name, pin.type, datas, pin.default_value);
          out += Indent(depth) + "return " + expr + ";\n";
        } else {
          std::vector<std::string> field_inits;
          for (const auto &pin : fn.decl.outputs)
            field_inits.push_back(ResolveInput(ctx, node_id, pin.name, pin.type, datas, pin.default_value));
          out += Indent(depth) + "return " + fn.cpp_name + "_Result{ " + Join(field_inits, ", ") + " };\n";
        }

      } else if (ctx.functions_by_id.count(type_id)) {
        const FunctionInfo &fn = ctx.functions_by_id.at(type_id);
        auto datas = GetNodeData(ctx, node_id);

        std::vector<std::string> arg_exprs;
        for (const auto &pin : fn.decl.inputs)
          arg_exprs.push_back(ResolveInput(ctx, node_id, pin.name, pin.type, datas, pin.default_value));

        std::string call_expr = fn.cpp_name + "(" + Join(arg_exprs, ", ") + ")";

        if (fn.decl.outputs.empty()) {
          out += Indent(depth) + call_expr + ";\n";
        } else if (fn.decl.outputs.size() == 1) {
          const auto &pin = fn.decl.outputs[0];
          std::string tmp = "call_" + SanitizeIdentifier(node_id);
          out += Indent(depth) + CppTypeForPinType(pin.type) + " " + tmp + " = " + call_expr + ";\n";
          ctx.call_result_vars[node_id][pin.name] = tmp;
        } else {
          std::string tmp = "call_" + SanitizeIdentifier(node_id);
          out += Indent(depth) + fn.cpp_name + "_Result " + tmp + " = " + call_expr + ";\n";
          for (const auto &pin : fn.decl.outputs)
            ctx.call_result_vars[node_id][pin.name] = tmp + "." + SanitizeIdentifier(pin.name);
        }

        for (const auto &next : GetNextNodes(ctx, node_id, "output_flow"))
          TranspileFlow(ctx, next, depth, out);
      } else {
        ctx.had_error = true;
        out += Indent(depth) + "// TODO: unsupported flow node type '" + type_id + "' (node " + node_id + ")\n";
      }

      ctx.visiting.erase(node_id);
    }

    void TranspileFunctionBody(TranspileCtx &ctx, FunctionInfo &fn, std::string &out_decls, std::string &out_structs) {
      GraphHandle prev_graph = ctx.graph;
      auto prev_visiting = std::move(ctx.visiting);
      auto prev_call_vars = std::move(ctx.call_result_vars);
      const FunctionInfo *prev_current_fn = ctx.current_function;
      std::string prev_start = ctx.current_function_start_schema;
      std::string prev_end = ctx.current_function_end_schema;

      ctx.graph = GraphHandle{ true, fn.folder_path };
      ctx.visiting.clear();
      ctx.call_result_vars.clear();
      ctx.current_function = &fn;
      ctx.current_function_start_schema = "input_foo_" + fn.decl.id;
      ctx.current_function_end_schema = "output_foo_" + fn.decl.id;

      std::string start_node = FindNodeBySchemaId(ctx, ctx.current_function_start_schema);

      std::string body;
      if (start_node.empty()) {
        ctx.had_error = true;
        body += Indent(1) + "// TODO: could not find the Start node for function '" + fn.decl.name + "' (" + fn.folder_path +
                ")\n";
      } else {
        for (const auto &next : GetNextNodes(ctx, start_node, "output"))
          TranspileFlow(ctx, next, 1, body);
      }

      std::string ret_type = "void";
      if (fn.decl.outputs.size() == 1)
        ret_type = CppTypeForPinType(fn.decl.outputs[0].type);
      else if (fn.decl.outputs.size() > 1)
        ret_type = fn.cpp_name + "_Result";

      if (fn.decl.outputs.size() > 1) {
        out_structs += "struct " + fn.cpp_name + "_Result {\n";
        for (const auto &pin : fn.decl.outputs)
          out_structs += "  " + CppTypeForPinType(pin.type) + " " + SanitizeIdentifier(pin.name) + ";\n";
        out_structs += "};\n\n";
      }

      std::vector<std::string> params;
      for (const auto &pin : fn.decl.inputs)
        params.push_back(CppTypeForPinType(pin.type) + " " + CppParamName(pin.name));

      out_decls += "// Function: " + fn.decl.name + " (" + fn.decl.id + ")\n";
      out_decls += ret_type + " " + fn.cpp_name + "(" + Join(params, ", ") + ") {\n";
      out_decls += body;
      if (ret_type != "void")
        out_decls += Indent(1) + "return {}; // fallback if no explicit return was reached in the graph\n";
      out_decls += "}\n\n";

      ctx.graph = prev_graph;
      ctx.visiting = std::move(prev_visiting);
      ctx.call_result_vars = std::move(prev_call_vars);
      ctx.current_function = prev_current_fn;
      ctx.current_function_start_schema = prev_start;
      ctx.current_function_end_schema = prev_end;
    }

  }  // namespace

  bool transpile_graph(const std::string &session_id, const std::string &storage_path, std::string &out_cpp_path) {
    if (session_id.empty()) {
      get_current_context()->m_interface->log_error("[TestCPP] Cannot transpile: no active graph session.");
      return false;
    }

    TranspileCtx ctx;
    ctx.graph = GraphHandle{ false, session_id };

    for (auto &v : TestCPP::load_variables_from_file(storage_path))
      ctx.vars_by_id[v.id] = v;

    std::filesystem::path storage_p(storage_path);
    std::filesystem::path base_dir = storage_p.has_parent_path() ? storage_p.parent_path() : std::filesystem::path(".");

    for (auto &f : TestCPP::load_functions_from_file(storage_path)) {
      FunctionInfo info;
      info.decl = f;
      info.folder_path = (base_dir / f.id).string();
      info.cpp_name = "fn_" + SanitizeIdentifier(f.id);
      ctx.functions_by_id[f.id] = std::move(info);
    }

    std::string on_setup_id = FindNodeBySchemaId(ctx, "on_setup");
    std::string on_loop_id = FindNodeBySchemaId(ctx, "on_loop");

    if (on_setup_id.empty()) {
      get_current_context()->m_interface->log_error("[TestCPP] Transpile failed: no 'on_setup' node found in the graph.");
      return false;
    }
    if (on_loop_id.empty()) {
      get_current_context()->m_interface->log_error("[TestCPP] Transpile failed: no 'on_loop' node found in the graph.");
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

    std::string functions_struct_decls;
    std::string functions_decls;
    for (auto &[id, info] : ctx.functions_by_id)
      TranspileFunctionBody(ctx, info, functions_decls, functions_struct_decls);

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
    file << "#include <chrono>\n";
    file << "#include <algorithm>\n";
    file << "#include <cstdlib>\n";
    file << "#include <cmath>\n";
    file << "#include <string>\n\n";

    file << "// Variables \n";
    file << var_decls << "\n";

    file << "// Function return-value structs\n";
    file << functions_struct_decls;

    file << "// Functions\n";
    file << functions_decls;

    file << "void OnSetup();\n";
    file << "void OnLoop();\n\n";

    file << "static bool need_close = false;\n\n";
    file << "void TriggerClose() {need_close = true;}\n\n";

    file << "int main() {\n";
    file << "  OnSetup();\n";
    file << "  while (true) {\n";
    file << "    OnLoop();\n";
    file << "    if(need_close) break;\n";
    file << "  }\n";
    file << "  return 0;\n";
    file << "}\n\n";

    file << "void OnSetup() {\n" << setup_body << "}\n\n";
    file << "void OnLoop() {\n" << loop_body << "}\n";

    std::filesystem::path out_p = storage_p.has_parent_path() ? storage_p.parent_path() / "generated_program.cpp"
                                                              : std::filesystem::path("generated_program.cpp");

    try {
      if (out_p.has_parent_path())
        std::filesystem::create_directories(out_p.parent_path());

      std::ofstream ofs(out_p);
      if (!ofs.is_open()) {
        get_current_context()->m_interface->log_error("[TestCPP] Could not open '" + out_p.string() + "' for writing.");
        return false;
      }
      ofs << file.str();
    } catch (const std::exception &e) {
      get_current_context()->m_interface->log_error(std::string("[TestCPP] Failed to write generated file: ") + e.what());
      return false;
    }

    out_cpp_path = out_p.string();

    if (ctx.had_error) {
      get_current_context()->m_interface->log_error(
          "[TestCPP] Transpile completed with warnings — check the TODO comments in " + out_cpp_path);
    }

    return true;
  }

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
      get_current_context()->m_interface->log_error("[TestCPP] Compilation failed (see console output above).");
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