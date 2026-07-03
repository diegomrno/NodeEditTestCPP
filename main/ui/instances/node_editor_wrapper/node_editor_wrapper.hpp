#pragma once
#include <vxcore/include/vortex.h>
#include <vxcore/include/vortex_internals.h>

#include "../../../src/helpers.hpp"

#ifndef NODE_EDITOR_WRAPPER_HPP
#define NODE_EDITOR_WRAPPER_HPP

namespace ModuleUI {
  class TestCppWrapperAppWindow : public std::enable_shared_from_this<TestCppWrapperAppWindow> {
   public:
    TestCppWrapperAppWindow(const std::string& name);

    void menubar();
    std::shared_ptr<Cherry::AppWindow>& get_app_window();
    static std::shared_ptr<TestCppWrapperAppWindow> create(const std::string& name);

    void set_instance_id(const std::string& gsid) {
      graph_session_id_ = gsid;
    }

   private:
    std::shared_ptr<VxContext> ctx;
    std::shared_ptr<Cherry::AppWindow> app_window_;
    bool opened;
    std::string graph_session_id_ = "undefined";
    std::string name_;
  };

  class DetailsWindow : public std::enable_shared_from_this<DetailsWindow> {
   public:
    DetailsWindow();

    void menubar();
    std::shared_ptr<Cherry::AppWindow>& get_app_window();
    static std::shared_ptr<DetailsWindow> create();
    void render();

    void setup_render_callback();
    bool i = false;

   private:
    std::shared_ptr<VxContext> ctx;
    std::shared_ptr<Cherry::AppWindow> app_window_;
    bool opened;
  };

  class DrawerWindow : public std::enable_shared_from_this<DrawerWindow> {
   public:
    DrawerWindow();

    void menubar();
    std::shared_ptr<Cherry::AppWindow>& get_app_window();
    static std::shared_ptr<DrawerWindow> create();
    void render();
    bool i = false;

    void setup_render_callback();

   private:
    std::shared_ptr<VxContext> ctx;
    std::shared_ptr<Cherry::AppWindow> app_window_;
    std::string gs_id_;
    bool opened;
  };
};  // namespace ModuleUI

#endif  // LOGUTILITY_H
