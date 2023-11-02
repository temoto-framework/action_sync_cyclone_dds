#include <class_loader/class_loader.hpp>
#include <iostream>

#include "dds/dds.hpp"
#include "temoto_action_engine/action_synchronizer_plugin_base.h"
#include "ActionSyncData.hpp"

using namespace org::eclipse::cyclonedds;

class action_sync_cyclone_dds : public ActionSynchronizerPluginBase
{
public:

  action_sync_cyclone_dds()
  {
    std::cout << __func__ << " constructed" << std::endl;
  }

  virtual void sendNotification(const Waitable& waitable, const std::string& result, const std::string& params)
  {
    std::cout << __func__ << ": " << waitable.action_name << ", etc" << std::endl;
  }
  
};

CLASS_LOADER_REGISTER_CLASS(action_sync_cyclone_dds, ActionSynchronizerPluginBase);