#include "publisher.hpp"
#include "subscriber.hpp"
#include "ActionSyncData.hpp"

#include <chrono>
#include <map>
#include <thread>

using namespace std::chrono;

class Participant
{

public:
    Participant(const std::string& actor_name)
    : actor_name_(actor_name)
    , sub_ready_("ready", std::bind(&Participant::readyCallback, this, std::placeholders::_1))
    , pub_ready_("ready")
    {}

    bool waitForConsensus(const std::string& graph_name, const std::vector<std::string> other_actors, size_t timeout)
    {
        ActionSyncData::Ready ready_msg;
        ready_msg.actor_name(actor_name_);
        ready_msg.graph_name(graph_name);

        bool consensus_reached{false};
        bool timeout_reached{false};
        auto start_time{high_resolution_clock::now()};
        unsigned int pub_count{0};

        std::thread ready_pub_thread{[&]
        {
            while(!consensus_reached && !timeout_reached)
            {
                ready_msg.timestamp(duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count());
                pub_ready_.publish(ready_msg);
                pub_count++;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }};

        // TODO: REMOVE. Wait until at least some msgs are sent. Replace by proper handshake protocol 
        while(pub_count < 4)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

        while(!consensus_reached && !timeout_reached)
        {
            auto current_time{high_resolution_clock::now()};
            if (duration_cast<milliseconds>(current_time - start_time).count() >= timeout)
            {
                timeout_reached = true;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::lock_guard<std::mutex> l(handshake_buffer_mutex_);

            auto hb_it{handshake_buffer_.find(graph_name)};
            if (hb_it == handshake_buffer_.end())
                continue;

            bool all_found{true};
            auto current_time_epoch{duration_cast<milliseconds>(current_time.time_since_epoch()).count()};
            for (const auto& other_actor : other_actors)
            {
                auto hb_actor_it{hb_it->second.find(other_actor)};
                if (hb_actor_it == hb_it->second.end())
                {
                    all_found = false;
                    continue;
                }

                if((int64_t)current_time_epoch - (int64_t)hb_actor_it->second >= (int64_t)timeout)
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
        return consensus_reached;
    }

private:
    temoto::Subscriber<ActionSyncData::Ready> sub_ready_;
    temoto::Publisher<ActionSyncData::Ready> pub_ready_;
    std::string actor_name_;
    std::map<std::string, std::map<std::string, uint64_t>> handshake_buffer_;
    std::mutex handshake_buffer_mutex_;

    void readyCallback(const ActionSyncData::Ready& msg)
    {
        if (msg.actor_name() == actor_name_)
            return;

        std::lock_guard<std::mutex> l(handshake_buffer_mutex_);
        if (handshake_buffer_.find(msg.graph_name()) == handshake_buffer_.end())
        {
            handshake_buffer_.insert
            ({
                msg.graph_name(),
                std::map<std::string, uint64_t>{{msg.actor_name(), msg.timestamp()}}
            });
            return;
        }

        handshake_buffer_[msg.graph_name()][msg.actor_name()] = msg.timestamp();
        return;
    }
};

int main(int argc, char** argv)
{
    std::string graph_name(argv[1]);
    std::string actor_name(argv[2]);
    size_t timeout{std::stoi(argv[3])};
    std::vector<std::string> other_actors;

    unsigned int count{4};
    while (argv[count] != nullptr)
        other_actors.push_back(argv[count++]);

    Participant p(actor_name);

    if (p.waitForConsensus(graph_name, other_actors, 5000))
    {
        std::cout << actor_name << ": Consensus reached for graph " << graph_name << std::endl;
        return 0;
    }
    else
    {
        std::cout << actor_name << ": Reached the timeout of " << timeout << " ms" << std::endl;
        return 1;
    }
}