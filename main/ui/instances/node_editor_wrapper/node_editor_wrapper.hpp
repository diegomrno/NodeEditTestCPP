#pragma once
#include <vxcore/include/vortex.h>
#include <vxcore/include/vortex_internals.h>

#include "../../../src/helpers.hpp"
#ifndef NODE_EDITOR_WRAPPER_HPP
#define NODE_EDITOR_WRAPPER_HPP
namespace ModuleUI {

  class TestCppWrapperAppWindow : public std::enable_shared_from_this<TestCppWrapperAppWindow> {
   public:
    TestCppWrapperAppWindow(const std::string &name, const std::string &id, const std::string &st);
    void menubar();
    std::shared_ptr<Cherry::AppWindow> &get_app_window();
    static std::shared_ptr<TestCppWrapperAppWindow>
    create(const std::string &name, const std::string &id, const std::string &st);
    void set_instance_id(const std::string &gsid) {
      graph_session_id_ = gsid;
    }

   private:
    std::shared_ptr<VxContext> ctx;
    std::shared_ptr<Cherry::AppWindow> app_window_;
    std::string storage_path_;
    bool opened;
    std::string id_;
    std::string graph_session_id_ = "undefined";
    std::string name_;
    bool refreshed = false;
    bool saved_ready = false;
  };

  class DetailsWindow : public std::enable_shared_from_this<DetailsWindow> {
   public:
    DetailsWindow(const std::string &parent_name, const std::string &id, const std::string &st);
    void menubar();
    std::shared_ptr<Cherry::AppWindow> &get_app_window();
    static std::shared_ptr<DetailsWindow>
    create(const std::string &parent_name, const std::string &id, const std::string &st);
    void render();
    void setup_render_callback();
    bool i = false;

   private:
    std::shared_ptr<VxContext> ctx;
    std::shared_ptr<Cherry::AppWindow> app_window_;
    std::shared_ptr<TestCPP::DrawerSession> drawer_session_ = nullptr;
    std::string parent_name_;
    std::string storage_path_;
    std::string id_;
    std::string gs_id_;
    bool refreshed = false;
    bool saved = false;
    bool opened;
    std::vector<std::string> available_types_;
    bool available_types_loaded_ = false;
  };

  class DrawerWindow : public std::enable_shared_from_this<DrawerWindow> {
   public:
    DrawerWindow(const std::string &parent_name, const std::string &id, const std::string &st);
    void menubar();
    std::shared_ptr<Cherry::AppWindow> &get_app_window();
    static std::shared_ptr<DrawerWindow>
    create(const std::string &parent_name, const std::string &id, const std::string &st);
    void render();
    bool i = false;
    void setup_render_callback();
    std::string open_function_graph(const std::string &function_id);
    void ShowVariablesPanel(const std::shared_ptr<TestCPP::DrawerSession> &session);

   private:
    std::shared_ptr<VxContext> ctx;
    std::shared_ptr<TestCPP::DrawerSession> drawer_session_ = nullptr;
    std::shared_ptr<Cherry::AppWindow> app_window_;
    std::string parent_name_;
    std::string id_;
    std::string storage_path_;
    std::string gs_id_ = "none";
    bool gs_id_loaded = false;
    bool refreshed = false;
    bool saved = false;
    bool opened;
    bool vars_loaded_from_file_ = false;
    bool initial_sync_done_ = false;
  };

  class ComponentsWindow : public std::enable_shared_from_this<ComponentsWindow> {
   public:
    ComponentsWindow(const std::string &parent_name, const std::string &id, const std::string &st);
    void menubar();
    std::shared_ptr<Cherry::AppWindow> &get_app_window();
    static std::shared_ptr<ComponentsWindow>
    create(const std::string &parent_name, const std::string &id, const std::string &st);
    void render();
    bool i = false;
    void setup_render_callback();

   private:
    std::shared_ptr<VxContext> ctx;
    std::shared_ptr<TestCPP::DrawerSession> drawer_session_ = nullptr;
    std::shared_ptr<Cherry::AppWindow> app_window_;
    std::string parent_name_;
    std::string id_;
    std::string storage_path_;
    std::string gs_id_ = "none";
    bool gs_id_loaded = false;
    bool refreshed = false;
    bool saved = false;
    bool opened;
    bool vars_loaded_from_file_ = false;
    bool initial_sync_done_ = false;
  };

};  // namespace ModuleUI
#endif  // NODE_EDITOR_WRAPPER_HPP