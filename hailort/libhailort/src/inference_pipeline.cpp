/**
 * Copyright (c) 2020-2022 Hailo Technologies Ltd. All rights reserved.
 * Distributed under the MIT license (https://opensource.org/licenses/MIT)
 **/
/**
 * @file inference_pipeline.cpp
 * @brief Implemention of inference pipeline
 **/

#include "hailo/inference_pipeline.hpp"
#include "common/async_thread.hpp"
#include "vstream_internal.hpp"
#include "hailort_defaults.hpp"
#include "context_switch/network_group_internal.hpp"

#include <sstream>

namespace hailort
{

InferVStreams::InferVStreams(std::vector<InputVStream> &&inputs, std::vector<OutputVStream> &&outputs, bool is_multi_context) :
    m_inputs(std::move(inputs)),
    m_outputs(std::move(outputs)),
    m_is_multi_context(is_multi_context)
{
    for (auto &input : m_inputs) {
        if (contains(m_network_name_to_input_count, input.network_name())) {
            ++m_network_name_to_input_count[input.network_name()];
        } else {
            m_network_name_to_input_count.emplace(input.network_name(), 1);
        }
    }
    for (auto &output : m_outputs) {
        if (contains(m_network_name_to_output_count, output.network_name())) {
            ++m_network_name_to_output_count[output.network_name()];
        } else {
            m_network_name_to_output_count.emplace(output.network_name(), 1);
        }
    }
}

hailo_status InferVStreams::verify_network_inputs_and_outputs(const std::map<std::string, MemoryView>& inputs_name_mem_view_map,
                                                   const std::map<std::string, MemoryView>& outputs_name_mem_view_map)
{
    std::map<std::string, std::pair<size_t, size_t>> input_output_count_per_network;

    for (const auto &input_name_to_memview : inputs_name_mem_view_map) {
        auto input_vstream = get_input_by_name(input_name_to_memview.first);
        CHECK_EXPECTED_AS_STATUS(input_vstream);
        auto network_name = input_vstream->get().network_name();
        if (contains(input_output_count_per_network, network_name)) {
            ++input_output_count_per_network[network_name].first;
        } else {
            input_output_count_per_network.emplace(network_name, std::pair<size_t, size_t>(1, 0));
        }
    }
    for (const auto &output_name_to_memview : outputs_name_mem_view_map) {
        auto output_vstream = get_output_by_name(output_name_to_memview.first);
        CHECK_EXPECTED_AS_STATUS(output_vstream);
        auto network_name = output_vstream->get().network_name();
        if (contains(input_output_count_per_network, network_name)) {
            ++input_output_count_per_network[network_name].second;
        } else {
            input_output_count_per_network.emplace(network_name, std::pair<size_t, size_t>(0, 1));
        }
    }
    CHECK(!m_is_multi_context || (input_output_count_per_network.size() == m_network_name_to_input_count.size()), HAILO_INVALID_ARGUMENT,
        "For multi-context network groups, inference is only supported on all available networks");

    for (const auto &network_to_input_output_count : input_output_count_per_network) {
        CHECK(network_to_input_output_count.second.first == m_network_name_to_input_count[network_to_input_output_count.first],
            HAILO_INVALID_ARGUMENT, "Not all inputs have been provided for network {}", network_to_input_output_count.first);
        CHECK(network_to_input_output_count.second.second == m_network_name_to_output_count[network_to_input_output_count.first],
            HAILO_INVALID_ARGUMENT, "Not all outputs have been provided for network {}", network_to_input_output_count.first);
    }
    return HAILO_SUCCESS;
}

static hailo_status verify_vstream_params_in_vstream_infos(const std::map<std::string, hailo_vstream_params_t> &params,
    const std::vector<hailo_vstream_info_t> &vstream_infos)
{
    for (const auto &name_to_param : params) {
        const auto &name = name_to_param.first;
        bool found = false;
        for (const auto &vstream_info : vstream_infos) {
            if (vstream_info.name == name) {
                found = true;
                break;
            }
        }
        CHECK(found, HAILO_NOT_FOUND, "Could not find vstream {}", name);
    }
    return HAILO_SUCCESS;
}

Expected<InferVStreams> InferVStreams::create(ConfiguredNetworkGroup &net_group,
        const std::map<std::string, hailo_vstream_params_t> &input_params,
        const std::map<std::string, hailo_vstream_params_t> &output_params)
{
    auto network_infos = net_group.get_network_infos();
    CHECK_EXPECTED(network_infos);
    auto is_multi_context = (dynamic_cast<ConfiguredNetworkGroupBase&>(net_group)).get_supported_features().multi_context;
    std::map<std::string, std::pair<size_t, size_t>> input_param_count_per_network;
    size_t total_inputs_found = 0;
    size_t total_outputs_found = 0;
    for (const auto &network_info : network_infos.value()) {
        auto input_vstream_infos_per_network = net_group.get_input_vstream_infos(network_info.name);
        CHECK_EXPECTED(input_vstream_infos_per_network);
        size_t input_counter = 0;
        for (const auto &vstream_info : input_vstream_infos_per_network.value()) {
            if (contains(input_params, std::string(vstream_info.name))) {
                ++input_counter;
                ++total_inputs_found;
            }
        }

        auto output_vstream_infos_per_network = net_group.get_output_vstream_infos(network_info.name);
        CHECK_EXPECTED(output_vstream_infos_per_network);
        size_t output_counter = 0;
        for (const auto &vstream_info : output_vstream_infos_per_network.value()) {
            if (contains(output_params, std::string(vstream_info.name))) {
                ++output_counter;
                ++total_outputs_found;
            }
        }

        if (0 != input_counter || 0 != output_counter) {
            CHECK_AS_EXPECTED(input_counter == input_vstream_infos_per_network->size(), HAILO_INVALID_ARGUMENT,
                "Found only partial inputs for network {}", network_info.name);
            CHECK_AS_EXPECTED(output_counter == output_vstream_infos_per_network->size(), HAILO_INVALID_ARGUMENT,
                "Found only partial outputs for network {}", network_info.name);
        } else {
            CHECK_AS_EXPECTED(!is_multi_context, HAILO_INVALID_ARGUMENT,
                "For multi-context network groups, the pipeline must be created for all available networks");
        }
    }

    if (total_inputs_found != input_params.size()) {
        auto all_input_vstream_infos = net_group.get_input_vstream_infos();
        CHECK_EXPECTED(all_input_vstream_infos);
        auto status = verify_vstream_params_in_vstream_infos(input_params, all_input_vstream_infos.release());
        CHECK_SUCCESS_AS_EXPECTED(status);
    }
    if (total_outputs_found != output_params.size()) {
        auto all_output_vstream_infos = net_group.get_input_vstream_infos();
        CHECK_EXPECTED(all_output_vstream_infos);
        auto status = verify_vstream_params_in_vstream_infos(output_params, all_output_vstream_infos.release());
        CHECK_SUCCESS_AS_EXPECTED(status);
    }

    auto input_vstreams = VStreamsBuilder::create_input_vstreams(net_group, input_params);
    CHECK_EXPECTED(input_vstreams);
    auto output_vstreams = VStreamsBuilder::create_output_vstreams(net_group, output_params);
    CHECK_EXPECTED(output_vstreams);

    return InferVStreams(input_vstreams.release(), output_vstreams.release(), is_multi_context);
}

hailo_status InferVStreams::infer(const std::map<std::string, MemoryView>& input_data,
    std::map<std::string, MemoryView>& output_data, size_t batch_size)
{
    auto status = verify_network_inputs_and_outputs(input_data, output_data);
    CHECK_SUCCESS(status);
    status = verify_memory_view_size(input_data, output_data, batch_size);
    CHECK_SUCCESS(status);

    std::vector<AsyncThreadPtr<hailo_status>> results;

    // Launch async read/writes
    for (auto &input_name_to_data_pair : input_data) {
        auto input_vstream_exp = get_input_by_name(input_name_to_data_pair.first);
        CHECK_EXPECTED_AS_STATUS(input_vstream_exp);
        auto &input_vstream = input_vstream_exp.release().get();
        results.emplace_back(std::make_unique<AsyncThread<hailo_status>>(
            [&input_vstream, &input_name_to_data_pair, batch_size]() -> hailo_status {
                const auto &input_buffer = input_name_to_data_pair.second;
                for (uint32_t i = 0; i < batch_size; i++) {
                    const size_t offset = i * input_vstream.get_frame_size();
                    auto status = input_vstream.write(MemoryView::create_const(
                        input_buffer.data() + offset,
                        input_vstream.get_frame_size()));
                    if (HAILO_STREAM_INTERNAL_ABORT == status) {
                        LOGGER__DEBUG("Input stream was aborted!");
                        return status;
                    }
                    CHECK_SUCCESS(status);
                }
                return HAILO_SUCCESS;
            }
        ));
    }
    for (auto &output_name_to_data_pair : output_data) {
        auto output_vstream_exp = get_output_by_name(output_name_to_data_pair.first);
        CHECK_EXPECTED_AS_STATUS(output_vstream_exp);
        auto &output_vstream = output_vstream_exp.release().get();
        results.emplace_back(std::make_unique<AsyncThread<hailo_status>>(
            [&output_vstream, &output_name_to_data_pair, batch_size]() {
                for (size_t i = 0; i < batch_size; i++) {
                    auto status = output_vstream.read(MemoryView(output_name_to_data_pair.second.data() + i * output_vstream.get_frame_size(), output_vstream.get_frame_size()));
                    if (HAILO_SUCCESS != status) {
                        return status;
                    }
                }
                return HAILO_SUCCESS;
            }
        ));
    }

    // Wait for all results
    auto error_status = HAILO_SUCCESS;
    for (auto& result : results) {
        status = result->get();
        if (HAILO_STREAM_INTERNAL_ABORT == status) {
            continue;
        }
        if (HAILO_SUCCESS != status) {
            error_status = status;
            LOGGER__ERROR("Failed waiting for threads with status {}", error_status);
        }
    }
    if (HAILO_SUCCESS != error_status) {
        return error_status;
    }

    return HAILO_SUCCESS;
}

hailo_status InferVStreams::verify_memory_view_size(const std::map<std::string, MemoryView>& inputs_name_mem_view_map,
    const std::map<std::string, MemoryView>& outputs_name_mem_view_map, size_t batch_count)
{
    for (const auto &input_name_to_memview : inputs_name_mem_view_map) {
        auto input_vstream_exp = get_input_by_name(input_name_to_memview.first);
        CHECK_EXPECTED_AS_STATUS(input_vstream_exp);
        auto &input_vstream = input_vstream_exp.release().get();
        CHECK(batch_count * input_vstream.get_frame_size() == input_name_to_memview.second.size(), HAILO_INVALID_ARGUMENT,
            "Memory size of vstream {} does not match the frame count! (Expected {}, got {})",
            input_vstream.name(), batch_count * input_vstream.get_frame_size(), input_name_to_memview.second.size());
    }
    for (const auto &output_name_to_memview : outputs_name_mem_view_map) {
        auto output_vstream_exp = get_output_by_name(output_name_to_memview.first);
        CHECK_EXPECTED_AS_STATUS(output_vstream_exp);
        auto &output_vstream = output_vstream_exp.release().get();
        CHECK(batch_count * output_vstream.get_frame_size() == output_name_to_memview.second.size(), HAILO_INVALID_ARGUMENT,
            "Memory size of vstream {} does not match the frame count! (Expected {}, got {})",
            output_vstream.name(), batch_count * output_vstream.get_frame_size(), output_name_to_memview.second.size());
    }

    return HAILO_SUCCESS;
}

Expected<std::reference_wrapper<InputVStream>> InferVStreams::get_input_by_name(const std::string &name)
{
    for (auto &input_vstream : m_inputs) {
        if (input_vstream.name() == name) {
            return std::ref(input_vstream);
        }
    }
    return make_unexpected(HAILO_NOT_FOUND);
}

Expected<std::reference_wrapper<OutputVStream>> InferVStreams::get_output_by_name(const std::string &name)
{
    for (auto &ouput_vstream : m_outputs) {
        if (ouput_vstream.name() == name) {
            return std::ref(ouput_vstream);
        }
    }
    return make_unexpected(HAILO_NOT_FOUND);
}

std::vector<std::reference_wrapper<InputVStream>> InferVStreams::get_input_vstreams()
{
    std::vector<std::reference_wrapper<InputVStream>> vsterams_refs;
    for (auto &input_vstream : m_inputs) {
        vsterams_refs.push_back(std::ref(input_vstream));
    }
    return vsterams_refs;
}

std::vector<std::reference_wrapper<OutputVStream>> InferVStreams::get_output_vstreams()
{
    std::vector<std::reference_wrapper<OutputVStream>> vsterams_refs;
    for (auto &ouput_vstream : m_outputs) {
        vsterams_refs.push_back(std::ref(ouput_vstream));
    }
    return vsterams_refs;
}

} /* namespace hailort */
