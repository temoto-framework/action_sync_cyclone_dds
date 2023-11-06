#include "dds/dds.hpp"
#include <thread>

using namespace org::eclipse::cyclonedds;

namespace temoto
{

template<typename T>
class Publisher
{
public:
    Publisher(const std::string& topic_name)
    : participant_(domain::default_id())
    , topic_(participant_, topic_name)
    , publisher_(participant_)
    , writer_(publisher_, topic_)
    {}

    void waitForSubscribers()
    {
        while (writer_.publication_matched_status().current_count() == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    void publish(const T& msg)
    {
        writer_.write(msg);
    }

private:
    dds::domain::DomainParticipant participant_;
    dds::topic::Topic<T> topic_;
    dds::pub::Publisher publisher_;
    dds::pub::DataWriter<T> writer_;
};
}