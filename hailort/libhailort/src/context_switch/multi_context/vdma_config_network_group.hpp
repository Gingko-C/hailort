/**
 * Copyright (c) 2020-2022 Hailo Technologies Ltd. All rights reserved.
 * Distributed under the MIT license (https://opensource.org/licenses/MIT)
 **/
/**
 * @file vdma_config_network_group.hpp
 * @brief Represent network_group from HEF file that can be activated 
 *
 * This network_group can be used for both single or multi context network_groups but for PCIE only
  **/

#ifndef _HAILO_CONTEXT_SWITCH_VDMA_CONFIG_NETWORK_GROUP_HPP_
#define _HAILO_CONTEXT_SWITCH_VDMA_CONFIG_NETWORK_GROUP_HPP_

#include "hailo/hailort.h"
#include "vdma_buffer.hpp"
#include "vdma_channel.hpp"
#include "vdma_descriptor_list.hpp"
#include "common/utils.hpp"
#include "context_switch/multi_context/vdma_config_activated_network_group.hpp"
#include "control_protocol.h"
#include "context_switch/active_network_group_holder.hpp"
#include "hailort_defaults.hpp"
#include "context_switch/network_group_internal.hpp"
#include "context_switch/multi_context/resource_manager.hpp"

#include <cstdint>
#include <assert.h>
#include <map>
#include <set>

namespace hailort
{

#define MAX_CONTEXTS_COUNT (CONTROL_PROTOCOL__MAX_TOTAL_CONTEXTS)


class VdmaConfigNetworkGroup : public ConfiguredNetworkGroupBase
{
public:
    static Expected<VdmaConfigNetworkGroup> create(VdmaConfigActiveAppHolder &active_net_group_holder,
        const ConfigureNetworkParams &config_params, 
        std::vector<std::shared_ptr<ResourcesManager>> resources_managers,
        std::shared_ptr<NetworkGroupMetadata> network_group_metadata);

    std::vector<std::shared_ptr<ResourcesManager>> &get_resources_managers()
    {
        return m_resources_managers;
    }

    hailo_status create_vdevice_streams_from_config_params();
    hailo_status create_output_vdevice_stream_from_config_params(
        const hailo_stream_parameters_t &stream_params, const std::string &stream_name);
    hailo_status create_input_vdevice_stream_from_config_params(
        const hailo_stream_parameters_t &stream_params, const std::string &stream_name);

    virtual Expected<std::unique_ptr<ActivatedNetworkGroup>> activate(
      const hailo_activate_network_group_params_t &network_group_params = HailoRTDefaults::get_network_group_params()) override;

    virtual Expected<hailo_stream_interface_t> get_default_streams_interface() override;

    virtual Expected<uint8_t> get_boundary_channel_index(uint8_t stream_index, hailo_stream_direction_t direction,
        const std::string &layer_name) override;

    virtual ~VdmaConfigNetworkGroup() = default;
    VdmaConfigNetworkGroup(const VdmaConfigNetworkGroup &other) = delete;
    VdmaConfigNetworkGroup &operator=(const VdmaConfigNetworkGroup &other) = delete;
    VdmaConfigNetworkGroup &operator=(VdmaConfigNetworkGroup &&other) = delete;
    VdmaConfigNetworkGroup(VdmaConfigNetworkGroup &&other) noexcept : ConfiguredNetworkGroupBase(std::move(other)),
      m_active_net_group_holder(other.m_active_net_group_holder),
      m_resources_managers(std::move(other.m_resources_managers)) {}

private:
    VdmaConfigNetworkGroup(VdmaConfigActiveAppHolder &active_net_group_holder,
        const ConfigureNetworkParams &config_params, 
        std::vector<std::shared_ptr<ResourcesManager>> &&resources_managers,
        const NetworkGroupMetadata &network_group_metadata, hailo_status &status);

    VdmaConfigActiveAppHolder &m_active_net_group_holder;
    std::vector<std::shared_ptr<ResourcesManager>> m_resources_managers;
};

} /* namespace hailort */

#endif /* _HAILO_CONTEXT_SWITCH_VDMA_CONFIG_NETWORK_GROUP_HPP_ */
