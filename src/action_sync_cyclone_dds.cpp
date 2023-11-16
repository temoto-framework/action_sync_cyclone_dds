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
    std::cout << __func__ << " constructed" << std::endl;
  }

  virtual bool notify(const Notification& notification, const std::set<std::string> other_actors, const size_t timeout)
  {
    ActionSyncData::Notification n;
    n.result(notification.result);
    n.parameters(notification.parameters);

    n.waitable(ActionSyncData::Waitable(
      notification.waitable.actor_name,
      notification.waitable.action_name,
      notification.waitable.graph_name));

    n.notification_id(notification.waitable.actor_name + "__" +
      notification.waitable.graph_name + "__" +
      notification.waitable.action_name + "__" +
      std::to_string(duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count())
    );

    bool handshakes_received{false};
    bool timeout_reached{false};
    auto start_time{high_resolution_clock::now()};
    std::string handshake_token = n.notification_id();
    std::vector<std::string> all_keys;

    for (const auto& oa : other_actors)
      all_keys.push_back(actor_name_ + "_" + oa);

    std::cout << "[" << actor_name_ << "] sending notification '" << n.notification_id() << "' and waiting for acks from ";

    for (const auto& k : all_keys)
      std::cout << k << ", ";

    std::cout << std::endl;

    // Create a new dummy entry
    {
      std::lock_guard<std::mutex> l(handshake_buffers_mutex_);
      handshake_buffers_.insert
      ({
        handshake_token,
        std::map<std::string, uint64_t>{{actor_name_ + "_" + actor_name_, 0}}
      });
    }

    /*
     * Start sending the notification messages
     */
    std::thread send_notifications_thread{[&]
    {
      while(!handshakes_received && !timeout_reached)
      {
        pub_notification_.publish(n);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }};

    /*
     * Periodically check if all actors got the noticication
     */
    while(!handshakes_received && !timeout_reached)
    {
      auto current_time{high_resolution_clock::now()};
      if (duration_cast<milliseconds>(current_time - start_time).count() >= timeout)
      {
        timeout_reached = true;
        break;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      std::lock_guard<std::mutex> l(handshake_buffers_mutex_);

      auto hb_it{handshake_buffers_.find(handshake_token)};
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
        handshakes_received = true;
        break;
      }
    }

    while(!send_notifications_thread.joinable()){}
    send_notifications_thread.join();

    std::cout << "[" << actor_name_ << "] D2. hr=" << handshakes_received << ", to=" << timeout_reached << std::endl;

    if (handshakes_received)
    {
      std::cout << "[" << actor_name_ << "] notification sent successfully '" << n.notification_id() << "'" << std::endl;
    }
    else
    {
      std::cout << "[" << actor_name_ << "] notification did not reach all actors '" << n.notification_id() << "'" << std::endl;
    }

    return handshakes_received;
  }

  virtual bool unidirHandshake(const std::string& handshake_token)
  {
    std::cout << "[" << actor_name_ << "] acknowledging '" << handshake_token << "'" << std::endl;

    ActionSyncData::Handshake handshake_msg;
    handshake_msg.type(ActionSyncData::HandshakeType::UNIDIRECTIONAL);
    handshake_msg.actor_name(actor_name_);
    handshake_msg.handshake_token(handshake_token);
    handshake_msg.timestamp(duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count());

    pub_handshake_.publish(handshake_msg);
    return true;
  }

  virtual bool bidirHandshake(const std::string& handshake_token, const std::set<std::string> other_actors, const size_t timeout)
  {
    std::cout << "[" << actor_name_ << "] performing handshake with ";
    for(const auto oa : other_actors)
    {
      std::cout << oa << ", ";
    }
    std::cout << std::endl;

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
      handshake_msg.type(ActionSyncData::HandshakeType::BIDIRECTIONAL);
      handshake_msg.actor_name(actor_name_);
      handshake_msg.handshake_token(handshake_token);

      while(!consensus_reached && !timeout_reached)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::unique_lock<std::mutex> l(handshake_buffers_mutex_);

        auto hb_it{handshake_buffers_.find(handshake_token)};
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

        l.unlock();
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

      auto hb_it{handshake_buffers_.find(handshake_token)};
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
    handshake_buffers_.erase(handshake_token);

    return consensus_reached;
  }

private:

  void handshakeCallback(const ActionSyncData::Handshake& msg)
  {
    // Ignore self
    if (msg.actor_name() == actor_name_ || actor_name_.empty())
        return;

    std::string key{actor_name_ + "_" + msg.actor_name()};
    std::lock_guard<std::mutex> l(handshake_buffers_mutex_);

    if (msg.type() == ActionSyncData::HandshakeType::BIDIRECTIONAL)
    {
      std::cout << "[" << actor_name_ << "] got handshake msg from '" << msg.actor_name() << "', with id '" << msg.handshake_token() << "'" << std::endl;
    }
    else if (msg.type() == ActionSyncData::HandshakeType::UNIDIRECTIONAL &&
      handshake_buffers_.find(msg.handshake_token()) != handshake_buffers_.end())
    {
      std::cout << "[" << actor_name_ << "] got acknowledgement from '" << msg.actor_name() << "', with id '" << msg.handshake_token() << "'" << std::endl;
    }

    /*
     * Update the local handshakes
     */
    if (handshake_buffers_.find(msg.handshake_token()) == handshake_buffers_.end())
    {
      handshake_buffers_.insert
      ({
        msg.handshake_token(),
        std::map<std::string, uint64_t>{{key, msg.timestamp()}}
      });
      return;
    }

    handshake_buffers_[msg.handshake_token()][key] = msg.timestamp();

    /*
     * Update the remote handshakes
     */
    for (const auto& a : msg.other_actors())
    {
      handshake_buffers_[msg.handshake_token()][msg.actor_name() + "_" + a.actor_name()] = a.timestamp();
    }

    return;
  }

  void notificationCallback(const ActionSyncData::Notification& msg)
  {
    // Ignore self and dont invoke uninitialized callback
    if (msg.waitable().actor_name() == actor_name_ || notification_received_cb_ == nullptr)
      return;

    std::lock_guard<std::mutex> l(processed_notification_mutex_); // Deliberately left closed for the whole scope

    // if (processsed_notifications_.find(msg.notification_id()) != processsed_notifications_.end())
    // {
    //   return;
    // }

    std::cout << "[" << actor_name_ << "] got notification '" << msg.notification_id() << "'" << std::endl;

    auto ret = processsed_notifications_.insert({
      msg.notification_id(), 
      duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count()
    });

    Notification n{
      .parameters = msg.parameters(),
      .result     = msg.result(),
      .id         = msg.notification_id(),
      .waitable   = Waitable{
        .action_name = msg.waitable().action_name(),
        .actor_name  = msg.waitable().actor_name(),
        .graph_name  = msg.waitable().graph_name()
      }
    };

    notification_received_cb_(n);
  }

  std::vector<std::string> getHandshakeKeys(const std::set<std::string>& other_actors)
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

  std::map<std::string, uint64_t> processsed_notifications_;
  std::mutex processed_notification_mutex_;
  
};

CLASS_LOADER_REGISTER_CLASS(action_sync_cyclone_dds, ActionSynchronizerPluginBase);