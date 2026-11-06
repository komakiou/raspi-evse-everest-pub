// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "powermeterImpl.hpp"

namespace module {
namespace powermeter {

void powermeterImpl::init() {
    power_on = false;
}

void powermeterImpl::ready() {
    thread_handle = std::thread(&powermeterImpl::thread, this);
}

types::powermeter::TransactionStartResponse
powermeterImpl::handle_start_transaction(types::powermeter::TransactionReq& value) {
    // your code for cmd start_transaction goes here
    EVLOG_info << "powermeterImpl::handle_start_transaction " << value.evse_id << " " << value.transaction_id;

    types::powermeter::TransactionStartResponse res;
    res.status = types::powermeter::TransactionRequestStatus::OK;

    return res;
}

types::powermeter::TransactionStopResponse powermeterImpl::handle_stop_transaction(std::string& transaction_id) {
    // your code for cmd stop_transaction goes here
    EVLOG_info << "powermeterImpl::handle_stop_transaction " << transaction_id;

    types::powermeter::TransactionStopResponse res;
    res.status = types::powermeter::TransactionRequestStatus::OK;

    return res;
}

void powermeterImpl::thread() {
    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        double volt_v = 0;
        double amp_a = 0;
        double freq_hz = 0;
        double power_w_import = 0;
        double energy_wh_import = 0;

        if (mod->enable_power_flow_sim) {
            std::uniform_real_distribution<double> distr_v(220, 240);
            std::uniform_real_distribution<double> distr_a(9, 11);
            std::uniform_real_distribution<double> distr_f(49, 51);

            volt_v = distr_v(gen);
            amp_a = distr_a(gen);
            freq_hz = distr_f(gen);

            power_w_import = volt_v * amp_a;

            energy_wh_import = power_w_import / 3600;
            energy_wh_import_total += energy_wh_import;
        }

        types::powermeter::Powermeter p;

        p.timestamp = Everest::Date::to_rfc3339(date::utc_clock::now());
        p.meter_id = "RaspiEvse_PowerMeter_Sim";
        p.phase_seq_error = false;

        p.energy_Wh_import.total = energy_wh_import_total;
        p.energy_Wh_import.L1 = energy_wh_import_total / 3;
        p.energy_Wh_import.L2 = energy_wh_import_total / 3;
        p.energy_Wh_import.L3 = energy_wh_import_total / 3;

        types::units::Power pwr;
        pwr.total = power_w_import;
        pwr.L1 = power_w_import / 3;
        pwr.L2 = power_w_import / 3;
        pwr.L3 = power_w_import / 3;
        p.power_W = pwr;

        types::units::Voltage volt;
        volt.L1 = volt_v;
        volt.L2 = volt_v;
        volt.L3 = volt_v;
        p.voltage_V = volt;

        types::units::Current amp;
        amp.L1 = amp_a / 3;
        amp.L2 = amp_a / 3;
        amp.L3 = amp_a / 3;
        amp.N = 3;
        p.current_A = amp;

        types::units::Frequency freq;
        freq.L1 = freq_hz;
        freq.L2 = freq_hz;
        freq.L3 = freq_hz;
        p.frequency_Hz = freq;

        publish_powermeter(p);
    }
}

} // namespace powermeter
} // namespace module
