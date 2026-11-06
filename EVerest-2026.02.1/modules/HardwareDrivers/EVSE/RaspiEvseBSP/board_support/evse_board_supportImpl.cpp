// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "evse_board_supportImpl.hpp"

#include <arpa/inet.h>
#include <chrono>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define SW_TYPE_EVSE 0
#define SW_TYPE_EV   1

#define I2C_REG_SW_TYPE  0x00
#define I2C_REG_SW_VER   0x01
#define I2C_REG_ADC_HIGH 0x02
#define I2C_REG_ADC_LOW  0x04
#define I2C_REG_DUTY     0x06

/**
 * i2c_open() - Open an I2C bus and bind it to a slave address.
 *
 * @bus_path:  Path to the I2C bus device, e.g. "/dev/i2c-1"
 * @addr:      7-bit I2C slave address
 *
 * Returns a file descriptor >= 0 on success, -1 on failure.
 */
static int i2c_open(const char* bus_path, uint8_t addr) {
    int fd = open(bus_path, O_RDWR);
    if (fd < 0) {
        perror("i2c_open: open");
        return -1;
    }

    /* Tell the kernel which slave device we want to talk to */
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        perror("i2c_open: ioctl I2C_SLAVE");
        close(fd);
        return -1;
    }

    return fd;
}

/**
 * Perform an atomic write+read using the ioctl interface.
 *
 * Some devices require the write and read to be a single I2C transaction
 * (no STOP condition between them — a "repeated START"). The I2C_RDWR ioctl
 * achieves this when plain write()+read() sequences don't work.
 *
 * @fd:       Open I2C file descriptor
 * @addr:     7-bit slave address
 * @reg:      Register to read from
 * @buf:      Buffer to store result
 * @len:      Number of bytes to read
 *
 * Returns 0 on success, -1 on failure.
 */
static int i2c_read_write(int fd, uint8_t addr, uint8_t reg, uint8_t* buf, uint16_t len) {
    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data data;

    /* Message 0: write the register address */
    msgs[0].addr = addr;
    msgs[0].flags = 0; /* write */
    msgs[0].len = 1;
    msgs[0].buf = &reg;

    /* Message 1: read the data (repeated START, no STOP between) */
    msgs[1].addr = addr;
    msgs[1].flags = I2C_M_RD; /* read */
    msgs[1].len = len;
    msgs[1].buf = buf;

    data.msgs = msgs;
    data.nmsgs = 2;

    if (ioctl(fd, I2C_RDWR, &data) < 0) {
        perror("i2c_read_write: ioctl I2C_RDWR");
        return -1;
    }
    return 0;
}

/**
 * i2c_write_reg() - Write a single byte to a register.
 *
 * Sends [reg, value] as one I2C write transaction.
 *
 * @fd:    Open I2C file descriptor
 * @reg:   Register address
 * @value: Byte to write
 *
 * Returns 0 on success, -1 on failure.
 */
static int i2c_write_reg(int fd, uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};

    if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
        perror("i2c_write_reg: write");
        return -1;
    }
    return 0;
}

namespace module {
namespace board_support {

void evse_board_supportImpl::init() {

    fd = i2c_open(mod->config.i2c_dev.c_str(), mod->config.i2c_addr);
    if (fd == -1) {
        EVLOG_error << "evse_board_supportImpl::init: i2c_open(" << mod->config.i2c_dev.c_str() << ") failed: " << fd;
        return;
    } else {
        EVLOG_info << "evse_board_supportImpl::init: i2c_open() success: " << fd;
    }

    uint8_t sw_type;
    uint8_t sw_ver;

    if (i2c_read_write(fd, mod->config.i2c_addr, I2C_REG_SW_TYPE, &sw_type, 1) != 0) {
        EVLOG_error << "evse_board_supportImpl::init: i2c_read_write(I2C_REG_SW_TYPE) failed";
    }

    if (i2c_read_write(fd, mod->config.i2c_addr, I2C_REG_SW_VER, &sw_ver, 1) != 0) {
        EVLOG_error << "evse_board_supportImpl::init: i2c_read_write(I2C_REG_SW_VER) failed";
    }

    EVLOG_info << "sw_ver: " << (int)sw_ver;
    EVLOG_info << "sw_type: " << (int)sw_type;

    if (sw_type != SW_TYPE_EVSE) {
        EVLOG_error << "Wrong HAT configuration! Expected SW_TYPE_EVSE but read " << (int)sw_type;
    } else {
        EVLOG_info << "HAT is configured as EVSE";
    }

    caps.max_current_A_import = 16;
    caps.min_current_A_import = 0;
    caps.max_phase_count_import = 1;
    caps.min_phase_count_import = 1;
    caps.max_current_A_export = 0;
    caps.min_current_A_export = 0;
    caps.max_phase_count_export = 1;
    caps.min_phase_count_export = 1;
    caps.supports_changing_phases_during_charging = false;
    caps.supports_cp_state_E = false;
    caps.connector_type = types::evse_board_support::Connector_type::IEC62196Type2Cable;
    caps.max_plug_temperature_C = 100;
}

void evse_board_supportImpl::ready() {
    EVLOG_info << "evse_board_supportImpl::ready()";

    i2c_read_thread_handle = std::thread(&evse_board_supportImpl::i2c_read_thread, this);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    publish_capabilities(caps);
}

void evse_board_supportImpl::handle_enable(bool& value) {
    // your code for cmd enable goes here
    EVLOG_info << "evse_board_supportImpl::handle_enable " << value;

    if (value == true) {
        uint8_t pwm_int = pwm;
        if (i2c_write_reg(fd, 6, pwm_int) != 0) {
            EVLOG_error << "evse_board_supportImpl::handle_enable: i2c_write_write() failed";
        }

    } else {
        uint8_t pwm_int = 0;
        if (i2c_write_reg(fd, 6, pwm_int) != 0) {
            EVLOG_error << "evse_board_supportImpl::handle_enable: i2c_write_write() failed";
        }
    }
}

void evse_board_supportImpl::handle_pwm_on(double& value) {
    // your code for cmd pwm_on goes here
    EVLOG_info << "evse_board_supportImpl::handle_pwm_on " << value;

    pwm = value;

    uint8_t pwm_int = 100 - pwm;
    if (i2c_write_reg(fd, I2C_REG_DUTY, pwm_int) != 0) {
        EVLOG_error << "evse_board_supportImpl::handle_enable: i2c_write_reg(I2C_REG_DUTY) failed";
    }
}

void evse_board_supportImpl::handle_cp_state_X1() {
    // your code for cmd cp_state_X1 goes here
    EVLOG_info << "evse_board_supportImpl::handle_cp_state_X1()";

    uint8_t pwm_int = 0;
    if (i2c_write_reg(fd, 6, pwm_int) != 0) {
        EVLOG_error << "evse_board_supportImpl::handle_cp_state_X1: i2c_write_write() failed";
    }
}

void evse_board_supportImpl::handle_cp_state_F() {
    // your code for cmd cp_state_F goes here
    EVLOG_info << "evse_board_supportImpl::handle_cp_state_F()";

    uint8_t pwm_int = 100;
    if (i2c_write_reg(fd, I2C_REG_DUTY, pwm_int) != 0) {
        EVLOG_error << "evse_board_supportImpl::handle_cp_state_F: i2c_write_write(I2C_REG_DUTY) failed";
    }
}

void evse_board_supportImpl::handle_cp_state_E() {
    // your code for cmd cp_state_E goes here
    EVLOG_error << "evse_board_supportImpl::handle_cp_state_E(): not implemented";
}

void evse_board_supportImpl::handle_allow_power_on(types::evse_board_support::PowerOnOff& value) {
    // your code for cmd allow_power_on goes here
    EVLOG_info << "evse_board_supportImpl::handle_allow_power_on " << value.allow_power_on;

    if (value.allow_power_on == true) {
        types::board_support_common::BspEvent event;
        event.event = types::board_support_common::Event::PowerOn;
        publish_event(event);

        mod->enable_power_flow_sim = true;
    } else {
        types::board_support_common::BspEvent event;
        event.event = types::board_support_common::Event::PowerOff;
        publish_event(event);

        mod->enable_power_flow_sim = false;
    }
}

void evse_board_supportImpl::handle_ac_switch_three_phases_while_charging(bool& value) {
    // your code for cmd ac_switch_three_phases_while_charging goes here
    EVLOG_info << "evse_board_supportImpl::handle_ac_switch_three_phases_while_charging " << value;
}

void evse_board_supportImpl::handle_ac_set_overcurrent_limit_A(double& value) {
    // your code for cmd ac_set_overcurrent_limit_A goes here
    EVLOG_info << "evse_board_supportImpl::handle_ac_set_overcurrent_limit_A " << value;
}

void evse_board_supportImpl::i2c_read_thread() {

    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        int16_t adc_pos;
        int16_t adc_neg;

        if (i2c_read_write(fd, mod->config.i2c_addr, I2C_REG_ADC_HIGH, (uint8_t*)&adc_pos, 2) != 0) {
            EVLOG_error << "evse_board_supportImpl::init: i2c_read_write(I2C_REG_ADC_HIGH) failed";
        }

        if (i2c_read_write(fd, mod->config.i2c_addr, I2C_REG_ADC_LOW, (uint8_t*)&adc_neg, 2) != 0) {
            EVLOG_error << "evse_board_supportImpl::init: i2c_read_write(I2C_REG_ADC_LOW) failed";
        }

        adc_pos = ntohs(adc_pos);
        adc_neg = ntohs(adc_neg);

        cp_pos_v = adc_pos * 13.2 / 1024;
        cp_neg_v = adc_neg * 13.2 / 1024;

        EVLOG_info << "CP: (" << adc_pos << ") " << cp_pos_v << " (" << adc_neg << ") " << cp_neg_v;

        types::board_support_common::BspEvent event;

        if (cp_pos_v > 10.5)
            event.event = types::board_support_common::Event::A;
        else if (cp_pos_v > 7.5)
            event.event = types::board_support_common::Event::B;
        else if (cp_pos_v > 4.5)
            event.event = types::board_support_common::Event::C;
        else if (cp_pos_v > 1.5)
            event.event = types::board_support_common::Event::D;
        else
            event.event = types::board_support_common::Event::E;

        publish_event(event);
    }
}

} // namespace board_support
} // namespace module
