/****************************************************************************
 *
 *   Copyright (c) 2018 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/*
 * ADIS16477.hpp
 *
 */

#ifndef DRIVERS_IMU_ADIS16477_ADIS16477_HPP_
#define DRIVERS_IMU_ADIS16477_ADIS16477_HPP_

#include <drivers/device/ringbuffer.h>
#include <drivers/device/spi.h>
#include <drivers/drv_hrt.h>
#include <drivers/drv_accel.h>
#include <drivers/drv_gyro.h>
#include <mathlib/math/filter/LowPassFilter2pVector3f.hpp>
#include <drivers/device/integrator.h>
#include <lib/conversion/rotation.h>
#include <perf/perf_counter.h>
#include <ecl/geo/geo.h>

#define ADIS16477_GYRO_DEFAULT_RATE			1000
#define ADIS16477_GYRO_DEFAULT_DRIVER_FILTER_FREQ	80

#define ADIS16477_ACCEL_DEFAULT_RATE			1000
#define ADIS16477_ACCEL_DEFAULT_DRIVER_FILTER_FREQ	30

class ADIS16477_gyro;

class ADIS16477 : public device::SPI
{
public:
	ADIS16477(int bus, const char *path_accel, const char *path_gyro, uint32_t device, enum Rotation rotation);
	virtual ~ADIS16477();

	virtual int		init();

	virtual int		ioctl(struct file *filp, int cmd, unsigned long arg);

	void			print_info();

protected:
	virtual int		probe();

	friend class ADIS16477_gyro;

	virtual int		gyro_ioctl(struct file *filp, int cmd, unsigned long arg);

private:
	ADIS16477_gyro		*_gyro{nullptr};

	uint16_t			_product{0};	/** product code */

	struct hrt_call		_call {};
	unsigned		_call_interval{1000};

	struct gyro_calibration_s	_gyro_scale {};

	// gyro 0.025 °/sec/LSB
	float				_gyro_range_scale{0.025f};
	float				_gyro_range_rad_s{math::radians(500.0f)};

	struct accel_calibration_s	_accel_scale {};

	// accel 1.25 mg/LSB
	float				_accel_range_scale{1.25f * CONSTANTS_ONE_G / 1000.0f};
	float				_accel_range_m_s2{40.0f * CONSTANTS_ONE_G};

	orb_advert_t		_accel_topic{nullptr};

	int					_accel_orb_class_instance{-1};
	int					_accel_class_instance{-1};

	unsigned			_sample_rate{1000};

	perf_counter_t		_sample_perf;
	perf_counter_t		_sample_interval_perf;

	perf_counter_t		_bad_transfers;

	math::LowPassFilter2pVector3f	_gyro_filter{ADIS16477_GYRO_DEFAULT_RATE, ADIS16477_GYRO_DEFAULT_DRIVER_FILTER_FREQ};
	math::LowPassFilter2pVector3f	_accel_filter{ADIS16477_ACCEL_DEFAULT_RATE, ADIS16477_ACCEL_DEFAULT_DRIVER_FILTER_FREQ};

	Integrator			_accel_int{4000, false};
	Integrator			_gyro_int{4000, true};

	enum Rotation		_rotation;

	perf_counter_t		_controller_latency_perf;

#pragma pack(push, 1)
	/**
	 * Report conversation with in the ADIS16477, including command byte and interrupt status.
	 */
	struct ADISReport {
		uint16_t	cmd;
		uint16_t	diag_stat;
		int16_t		gyro_x;
		int16_t		gyro_y;
		int16_t		gyro_z;
		int16_t		accel_x;
		int16_t		accel_y;
		int16_t		accel_z;
		uint16_t	temp;
		uint16_t	DATA_CNTR;
		uint8_t		checksum;
		uint8_t		_padding; // 16 bit SPI mode
	};
#pragma pack(pop)

	/**
	 * Start automatic measurement.
	 */
	void		start();

	/**
	 * Stop automatic measurement.
	 */
	void		stop();

	/**
	 * Reset chip.
	 *
	 * Resets the chip and measurements ranges, but not scale and offset.
	 */
	int			reset();

	/**
	 * Static trampoline from the hrt_call context; because we don't have a
	 * generic hrt wrapper yet.
	 *
	 * Called by the HRT in interrupt context at the specified rate if
	 * automatic polling is enabled.
	 *
	 * @param arg		Instance pointer for the driver that is polling.
	 */
	static void		measure_trampoline(void *arg);

	static int		data_ready_interrupt(int irq, void *context, void *arg);

	/**
	 * Fetch measurements from the sensor and update the report buffers.
	 */
	int			measure();

	void			publish_accel(const hrt_abstime &t, const ADISReport &report);
	void			publish_gyro(const hrt_abstime &t, const ADISReport &report);

	uint16_t		read_reg16(uint8_t reg);

	void			write_reg(uint8_t reg, uint8_t value);
	void			write_reg16(uint8_t reg, uint16_t value);

	// ADIS16477 onboard self test
	bool 			self_test_memory();
	bool 			self_test_sensor();

	ADIS16477(const ADIS16477 &);
	ADIS16477 operator=(const ADIS16477 &);
};

#endif /* DRIVERS_IMU_ADIS16477_ADIS16477_HPP_ */
