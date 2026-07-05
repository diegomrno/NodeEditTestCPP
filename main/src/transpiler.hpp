#pragma once
#include <string>

namespace TestCPP {
  bool transpile_graph(const std::string &session_id, const std::string &storage_path, std::string &out_cpp_path);
  bool compile_and_run(const std::string &cpp_path);
}  // namespace TestCPP