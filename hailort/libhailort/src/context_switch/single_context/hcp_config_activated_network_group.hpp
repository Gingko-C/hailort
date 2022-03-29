/**
 * Copyright (c) 2020-2022 Hailo Technologies Ltd. All rights reserved.
 * Distributed under the MIT license (https://opensource.org/licenses/MIT)
 **/
/**
 * @file hcp_config_activated_network_group.hpp
 * @brief Represent activated network_group from HEF. 
 *
 * This network_group can be used for control-cofigured network_groups only (for etherent or pcie)
  **/

#ifndef _HAILO_CONTEXT_SWITCH_HCP_CONFIG_ACTIVATED_NETWORK_GROUP_HPP_
#define _HAILO_CONTEXT_SWITCH_HCP_CONFIG_ACTIVATED_NETWORK_GROUP_HPP_

#include "hailo/device.hpp"
#include "common/utils.hpp"
#include "context_switch/network_group_internal.hpp"
#include "context_switch/active_network_group_holder.hpp"

#include <vector>
#include <map>

namespace hailort
{

struct WriteMemoryInfo
{
    uint32_t address;
    Buffer data;
};

class HcpConfigActivatedNetworkGroup;
using HcpConfigActiveAppHolder = ActiveNetworkGroupHolder<HcpConfigActivatedNetworkGroup>;

class HcpConfigActivatedNetworkGroup : public ActivatedNetworkGroupBase
{
  public:
    static Expected<HcpConfigActivatedNetworkGroup> create(Device &device, std::vector<WriteMemoryInfo> &config,
        const hailo_activate_network_group_params_t &network_group_params,
        std::map<std::string, std::unique_ptr<InputStream>> &input_streams,
        std::map<std::string, std::unique_ptr<OutputStream>> &output_streams,        
        HcpConfigActiveAppHolder &active_net_group_holder,
        hailo_power_mode_t power_mode, EventPtr network_group_activated_event);

    virtual ~HcpConfigActivatedNetworkGroup();
    HcpConfigActivatedNetworkGroup(const HcpConfigActivatedNetworkGroup &) = delete;
    HcpConfigActivatedNetworkGroup &operator=(const HcpConfigActivatedNetworkGroup &) = delete;
    HcpConfigActivatedNetworkGroup &operator=(HcpConfigActivatedNetworkGroup &&) = delete;
    HcpConfigActivatedNetworkGroup(HcpConfigActivatedNetworkGroup &&other) noexcept :
      ActivatedNetworkGroupBase(std::move(other)), m_active_net_group_holder(other.m_active_net_group_holder),
      m_is_active(std::exchange(other.m_is_active, false)), m_power_mode(other.m_power_mode),
      m_device(other.m_device) {};

    virtual Expected<Buffer> get_intermediate_buffer(const IntermediateBufferKey &/*key*/) override
    {
        LOGGER__ERROR("get_intermediate_buffer() is not supported on single_context network_groups");
        return make_unexpected(HAILO_INVALID_OPERATION);
    }


  private:
      HcpConfigActivatedNetworkGroup(Device &device, HcpConfigActiveAppHolder &active_net_group_holder,
        const hailo_activate_network_group_params_t &network_group_params,
        std::map<std::string, std::unique_ptr<InputStream>> &input_streams,
        std::map<std::string, std::unique_ptr<OutputStream>> &output_streams,        
        hailo_power_mode_t power_mode, EventPtr &&network_group_activated_event, hailo_status &status);

    HcpConfigActiveAppHolder &m_active_net_group_holder;
    bool m_is_active;
    hailo_power_mode_t m_power_mode;
    Device &m_device;
};

} /* namespace hailort */

#endif /* _HAILO_CONTEXT_SWITCH_HCP_CONFIG_ACTIVATED_NETWORK_GROUP_HPP_ */
