#include "dds/dds.hpp"

using namespace org::eclipse::cyclonedds;

namespace temoto
{

template<typename T> using CallbackType = std::function<void(const T&)>;

template<typename T>
class DataListener: public dds::sub::NoOpDataReaderListener<T>
{
public:
    DataListener(CallbackType<T> callback)
    : callback_ {callback}
    {}

private:
    void on_data_available(dds::sub::DataReader<T>& reader)
    {
        dds::sub::LoanedSamples<T> samples = reader.take();

        if (samples.length() == 0)
            return;

        for (const auto& sample : samples)
        {
            if (!sample.info().valid())
            {
                std::cout << __func__ << ": invalid sample" << std::endl;
                continue;
            }

            callback_(sample.data());
        }
    }

    CallbackType<T> callback_;
};

template<typename T>
class Subscriber
{
public:
    Subscriber(const std::string& topic_name, CallbackType<T> callback)
    : participant_(domain::default_id())
    , topic_(participant_, topic_name)
    , subscriber_(participant_)
    , listener_(callback)
    , reader_(subscriber_, topic_, drqos_, &listener_, dds::core::status::StatusMask::data_available())
    {}

private:
    dds::domain::DomainParticipant participant_;
    dds::topic::Topic<T> topic_;
    dds::sub::Subscriber subscriber_;
    dds::sub::qos::DataReaderQos drqos_;
    DataListener<T> listener_;
    dds::sub::DataReader<T> reader_;
};
}