// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#include "RaspiEvBSP.hpp"

namespace module {

void RaspiEvBSP::init() {
    invoke_init(*p_main);
    EVLOG_info << "RaspiEvBSP::init()";
}

void RaspiEvBSP::ready() {
    invoke_ready(*p_main);
    EVLOG_info << "RaspiEvBSP::ready()";
}

} // namespace module
