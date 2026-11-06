// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include "ev_board_supportImpl.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define SW_TYPE_EVSE 0
#define SW_TYPE_EV   1

#define I2C_REG_SW_TYPE    0x00
#define I2C_REG_SW_VER     0x01
#define I2C_REG_ADC_HIGH   0x02
#define I2C_REG_ADC_LOW    0x04
#define I2C_REG_PWM_PERIOD 0x06
#define I2C_REG_PWM_HIGH   0x08
#define I2C_REG_SW2        0x0A

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
namespace main {

void ev_board_supportImpl::init() {
    EVLOG_info << "ev_board_supportImpl::init()";

    fd = i2c_open(mod->config.i2c_dev.c_str(), mod->config.i2c_addr);
    if (fd == -1) {
        EVLOG_error << "ev_board_supportImpl::init:  i2c_open(" << mod->config.i2c_dev.c_str() << ") failed: " << fd;
        return;
    } else {
        EVLOG_info << "ev_board_supportImpl::init:  i2c_open() success: " << fd;
    }

    uint8_t sw_type;
    uint8_t sw_ver;

    if (i2c_read_write(fd, mod->config.i2c_addr, I2C_REG_SW_TYPE, &sw_type, 1) != 0) {
        EVLOG_error << "ev_board_supportImpl::init:  i2c_read_write(I2C_REG_SW_TYPE) failed";
    }

    if (i2c_read_write(fd, mod->config.i2c_addr, I2C_REG_SW_VER, &sw_ver, 1) != 0) {
        EVLOG_error << "ev_board_supportImpl::init:  i2c_read_write(I2C_REG_SW_VER) failed";
    }

    EVLOG_info << "sw_ver: " << (int)sw_ver;
    EVLOG_info << "sw_type: " << (int)sw_type;

    if (sw_type != SW_TYPE_EV) {
        EVLOG_error << "Wrong HAT configuration! Expected SW_TYPE_EV but read " << sw_type;
    } else {
        EVLOG_info << "HAT is configured as EV";
    }
}

void ev_board_supportImpl::ready() {
    EVLOG_info << "ev_board_supportImpl::ready()";

    i2c_read_thread_handle = std::thread(&ev_board_supportImpl::i2c_read_thread, this);
}

void ev_board_supportImpl::handle_enable(bool& value) {
    // your code for cmd enable goes here
    EVLOG_info << "ev_board_supportImpl::handle_enable(" << value << ")";
}

void ev_board_supportImpl::handle_set_cp_state(types::ev_board_support::EvCpState& cp_state) {
    // your code for cmd set_cp_state goes here
    switch (cp_state) {
    case types::ev_board_support::EvCpState::A:
        EVLOG_info << "ev_board_supportImpl::handle_set_cp_state(A)";
        break;
    case types::ev_board_support::EvCpState::B:
        EVLOG_info << "ev_board_supportImpl::handle_set_cp_state(B)";
        break;
    case types::ev_board_support::EvCpState::C:
        EVLOG_info << "ev_board_supportImpl::handle_set_cp_state(C)";
        break;
    case types::ev_board_support::EvCpState::D:
        EVLOG_info << "ev_board_supportImpl::handle_set_cp_state(D)";
        break;
    case types::ev_board_support::EvCpState::E:
        EVLOG_info << "ev_board_supportImpl::handle_set_cp_state(E)";
        break;
    }
}

void ev_board_supportImpl::handle_allow_power_on(bool& value) {
    // your code for cmd allow_power_on goes here
    EVLOG_info << "ev_board_supportImpl::handle_allow_power_on(" << value << ")";
}

void ev_board_supportImpl::handle_diode_fail(bool& value) {
    // your code for cmd diode_fail goes here
    EVLOG_info << "ev_board_supportImpl::handle_diode_fail(" << value << ")";
}

void ev_board_supportImpl::handle_set_ac_max_current(double& current) {
    // your code for cmd set_ac_max_current goes here
    EVLOG_info << "ev_board_supportImpl::handle_set_ac_max_current(" << current << ")";
}

void ev_board_supportImpl::handle_set_three_phases(bool& three_phases) {
    // your code for cmd set_three_phases goes here
    EVLOG_info << "ev_board_supportImpl::handle_set_three_phases(" << three_phases << ")";
}

void ev_board_supportImpl::handle_set_rcd_error(double& rcd_current_mA) {
    // your code for cmd set_rcd_error goes here
    EVLOG_info << "ev_board_supportImpl::handle_set_rcd_error(" << rcd_current_mA << ")";
}

void ev_board_supportImpl::i2c_read_thread() {

    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        EVLOG_info << "ev_board_supportImpl::i2c_read_thread()";

        int16_t adc_cp_hi = 0;
        int16_t adc_cp_lo = 0;

        if (i2c_read_write(fd, mod->config.i2c_addr, I2C_REG_ADC_HIGH, (uint8_t*)&adc_cp_hi, 2) != 0) {
            EVLOG_error << "i2c_read_write(I2C_REG_ADC_HIGH) failed";
        }

        if (i2c_read_write(fd, mod->config.i2c_addr, I2C_REG_ADC_LOW, (uint8_t*)&adc_cp_lo, 2) != 0) {
            EVLOG_error << "i2c_read_write(I2C_REG_ADC_LOW) failed";
        }

        adc_cp_hi = ntohs(adc_cp_hi);
        adc_cp_lo = ntohs(adc_cp_lo);

        cp_hi_v = adc_cp_hi * 13.2 / 1024;
        cp_lo_v = adc_cp_lo * 13.2 / 1024;

        EVLOG_info << "cp_hi_v: " << cp_hi_v << " cp_lo_v: " << cp_lo_v;

        uint16_t pwm_period = 0;
        uint16_t pwm_high = 0;

        if (i2c_read_write(fd, mod->config.i2c_addr, I2C_REG_PWM_PERIOD, (uint8_t*)&pwm_period, 2) != 0) {
            EVLOG_error << "i2c_read_write(I2C_REG_PWM_PERIOD) failed";
        }

        if (i2c_read_write(fd, mod->config.i2c_addr, I2C_REG_PWM_HIGH, (uint8_t*)&pwm_high, 2) != 0) {
            EVLOG_error << "i2c_read_write(I2C_REG_PWM_HIGH) failed";
        }

        pwm_period = ntohs(pwm_period);
        pwm_high = ntohs(pwm_high);

        cp_freq = 1.0 / ((double)pwm_period * 1.0 / 16000000.0);
        cp_duty = 100.0 - ((double)pwm_high / (double)pwm_period) * 100.0;

        EVLOG_info << "cp_freq: " << cp_freq << " cp_duty: " << cp_duty << " pwm_period: " << pwm_period
                   << " pwm_high: " << pwm_high;

        types::board_support_common::BspEvent event;

        if (cp_hi_v > 10.5)
            event.event = types::board_support_common::Event::A;
        else if (cp_hi_v > 7.5)
            event.event = types::board_support_common::Event::B;
        else if (cp_hi_v > 4.5)
            event.event = types::board_support_common::Event::C;
        else if (cp_hi_v > 1.5)
            event.event = types::board_support_common::Event::D;
        else
            event.event = types::board_support_common::Event::A;

        publish_bsp_event(event);

        types::board_support_common::BspMeasurement bspm;
        if (cp_freq < 1500 && cp_freq > 500) {
            bspm.cp_pwm_duty_cycle = cp_duty;
            bspm.proximity_pilot = {types::board_support_common::Ampacity::A_13};
        } else {
            bspm.cp_pwm_duty_cycle = 0;
            bspm.proximity_pilot = {types::board_support_common::Ampacity::None};
        }

        publish_bsp_measurement(bspm);
    }
}

} // namespace main
} // namespace module
