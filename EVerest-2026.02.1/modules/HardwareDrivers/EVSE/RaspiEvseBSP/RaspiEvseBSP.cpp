// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#include "RaspiEvseBSP.hpp"

namespace module {

void RaspiEvseBSP::init() {
    invoke_init(*p_powermeter);
    invoke_init(*p_board_support);
    invoke_init(*p_rcd);
    invoke_init(*p_connector_lock);

    EVLOG_info << "RaspiEvseBSP::init()";
    EVLOG_info << "i2c_freq: " << config.i2c_freq;
    EVLOG_info << "i2c_dev: " << config.i2c_dev;
    EVLOG_info << "i2c_addr: " << config.i2c_addr;
    EVLOG_info << "caps_min_current_A: " << config.caps_min_current_A;
    EVLOG_info << "caps_max_current_A: " << config.caps_max_current_A;

    enable_power_flow_sim = false;
}

void RaspiEvseBSP::ready() {
    invoke_ready(*p_powermeter);
    invoke_ready(*p_board_support);
    invoke_ready(*p_rcd);
    invoke_ready(*p_connector_lock);

    EVLOG_info << "RaspiEvseBSP::ready()";
}

} // namespace module
