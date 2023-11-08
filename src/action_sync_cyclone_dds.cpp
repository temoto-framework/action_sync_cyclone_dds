/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright 2023 TeMoto Framework
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <chrono>
#include <class_loader/class_loader.hpp>
#include <iostream>
#include <map>
#include <thread>

#include "ActionSyncData.hpp"
#include "publisher.hpp"
#include "subscriber.hpp"
#include "temoto_action_engine/action_synchronizer_plugin_base.h"

using namespace std::chrono;
// using HandshakeBuffer = std::map<std::string, std::vector<std::pair<std::string, uint64_t>>>;

class action_sync_cyclone_dds : public ActionSynchronizerPluginBase
{
public:

  action_sync_cyclone_dds() 
  : sub_handshake_("handshake", std::bind(&action_sync_cyclone_dds::handshakeCallback, this, std::placeholders::_1))
  , pub_handshake_("handshake")
  , sub_notification_("notification", std::bind(&action_sync_cyclone_dds::notificationCallback, this, std::placeholders::_1))
  , pub_notification_("notification")
  {
    setName("NO_NAME");
    std::cout << __func__ << " constructed" << std::endl;
  }

  virtual void sendNotification(const Waitable& waitable, const std::string& result, const std::string& params)
  {
    std::cout << __func__ << ": " << waitable.action_name << ", etc" << std::endl;
  }

  virtual bool waitForConsensus(const std::string& graph_name, const std::vector<std::string> other_actors, size_t timeout)
  {
    auto all_keys{getHandshakeKeys(other_actors)};
    bool consensus_reached{false};
    bool timeout_reached{false};
    auto start_time{high_resolution_clock::now()};

    /*
     * Start concurrently publishing locally registered handshakes
     */
    std::thread ready_pub_thread{[&]
    {
      ActionSyncData::Handshake handshake_msg;
      handshake_msg.actor_name(actor_name_);
      handshake_msg.graph_name(graph_name);

      while(!consensus_reached && !timeout_reached)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::lock_guard<std::mutex> l(handshake_buffers_mutex_);

        auto hb_it{handshake_buffers_.find(graph_name)};
        if (hb_it != handshake_buffers_.end())
        {
          std::vector<ActionSyncData::NameStamped> other_handshakes;
          for (const auto& a : other_actors)
          {
            std::string key{actor_name_ + "_" + a};
            if (hb_it->second.find(key) != hb_it->second.end())
              other_handshakes.push_back(ActionSyncData::NameStamped(a, hb_it->second[key]));
          }

          handshake_msg.other_actors(other_handshakes);
        }

        handshake_msg.timestamp(duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count());
        pub_handshake_.publish(handshake_msg);
      }
    }};

    /*
     * Periodically check if all participants have heared from eachother
     */
    while(!consensus_reached && !timeout_reached)
    {
      auto current_time{high_resolution_clock::now()};
      if (duration_cast<milliseconds>(current_time - start_time).count() >= timeout)
      {
        timeout_reached = true;
        break;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      std::lock_guard<std::mutex> l(handshake_buffers_mutex_);

      auto hb_it{handshake_buffers_.find(graph_name)};
      if (hb_it == handshake_buffers_.end())
        continue;

      bool all_found{true};
      auto current_time_epoch{duration_cast<milliseconds>(current_time.time_since_epoch()).count()};
      for (const auto& key : all_keys)
      {
        auto hb_key_it{hb_it->second.find(key)};

        if (hb_key_it == hb_it->second.end() ||
          (int64_t)current_time_epoch - (int64_t)hb_key_it->second >= (int64_t)timeout)
        {
          all_found = false;
          continue;
        }
      }

      if (all_found)
      {
        consensus_reached = true;
        break;
      }
    }

    while(!ready_pub_thread.joinable()){}
    ready_pub_thread.join();

    std::lock_guard<std::mutex> l(handshake_buffers_mutex_);
    handshake_buffers_.erase(graph_name);

    return consensus_reached;
  }

private:

  void handshakeCallback(const ActionSyncData::Handshake& msg)
  {
    if (msg.actor_name() == actor_name_)
        return;

    std::string key{actor_name_ + "_" + msg.actor_name()};
    std::lock_guard<std::mutex> l(handshake_buffers_mutex_);

    /*
     * Update the local handshakes
     */
    if (handshake_buffers_.find(msg.graph_name()) == handshake_buffers_.end())
    {
      handshake_buffers_.insert
      ({
        msg.graph_name(),
        std::map<std::string, uint64_t>{{key, msg.timestamp()}}
      });
      return;
    }

    handshake_buffers_[msg.graph_name()][key] = msg.timestamp();

    /*
     * Update the remote handshakes
     */
    for (const auto& a : msg.other_actors())
    {
      handshake_buffers_[msg.graph_name()][msg.actor_name() + "_" + a.actor_name()] = a.timestamp();
    }

    return;
  }

  void notificationCallback(const ActionSyncData::Notification& msg)
  {
  }

  std::vector<std::string> getHandshakeKeys(const std::vector<std::string>& other_actors)
  {
    std::vector<std::string> keys;

    /*
     * Generate "self_other", "other_self", and "other_other" combinations
     */
    for (const auto& oa : other_actors)
    {
      keys.push_back(actor_name_ + "_" + oa); // "self_other"
      keys.push_back(oa + "_" + actor_name_); // "other_self"

      for (const auto& ooa : other_actors)    // "other_other"
      {
        if (oa != ooa)
          keys.push_back(oa + "_" + ooa);
      }
    }

    return keys;
  }

  temoto::Subscriber<ActionSyncData::Handshake> sub_handshake_;
  temoto::Publisher<ActionSyncData::Handshake> pub_handshake_;

  temoto::Subscriber<ActionSyncData::Notification> sub_notification_;
  temoto::Publisher<ActionSyncData::Notification> pub_notification_;

  std::map<std::string, std::map<std::string, uint64_t>> handshake_buffers_;
  std::mutex handshake_buffers_mutex_;
  
};

CLASS_LOADER_REGISTER_CLASS(action_sync_cyclone_dds, ActionSynchronizerPluginBase);