/*
 * Copyright (c) 2018 qianfan Zhao
 * Copyright (c) 2018, 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sample_usbd.h>

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/irq.h>
#include <hal/nrf_gpiote.h>

#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_hid.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Define buttons for direct GPIO testing */
static const struct gpio_dt_spec button0 = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
static const struct gpio_dt_spec button2 = GPIO_DT_SPEC_GET(DT_ALIAS(sw2), gpios);
static const struct gpio_dt_spec button3 = GPIO_DT_SPEC_GET(DT_ALIAS(sw3), gpios);

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const uint8_t hid_report_desc[] = HID_MOUSE_REPORT_DESC(2);

#define MOUSE_BTN_LEFT		0
#define MOUSE_BTN_RIGHT		1

enum mouse_report_idx {
	MOUSE_BTN_REPORT_IDX = 0,
	MOUSE_X_REPORT_IDX = 1,
	MOUSE_Y_REPORT_IDX = 2,
	MOUSE_WHEEL_REPORT_IDX = 3,
	MOUSE_REPORT_COUNT = 4,
};

K_MSGQ_DEFINE(mouse_msgq, MOUSE_REPORT_COUNT, 2, 1);
static bool mouse_ready;

/* GPIO interrupt callback data */
static struct gpio_callback button0_cb_data;
static struct gpio_callback button1_cb_data;
static struct gpio_callback button2_cb_data;
static struct gpio_callback button3_cb_data;

/* GPIO interrupt handlers */
static void button0_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	LOG_INF("*** BUTTON0 INTERRUPT TRIGGERED! pins=0x%x ***", pins);
	gpio_pin_toggle_dt(&led0);
	
	/* Send mouse event */
	static uint8_t tmp[MOUSE_REPORT_COUNT] = {0};
	tmp[MOUSE_BTN_REPORT_IDX] = 0x01; // Left button pressed
	k_msgq_put(&mouse_msgq, tmp, K_NO_WAIT);
}

static void button1_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	LOG_INF("*** BUTTON1 INTERRUPT TRIGGERED! pins=0x%x ***", pins);
	
	/* Send mouse event */
	static uint8_t tmp[MOUSE_REPORT_COUNT] = {0};
	tmp[MOUSE_BTN_REPORT_IDX] = 0x02; // Right button pressed
	k_msgq_put(&mouse_msgq, tmp, K_NO_WAIT);
}

static void button2_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	LOG_INF("*** BUTTON2 INTERRUPT TRIGGERED! pins=0x%x ***", pins);
	
	/* Send mouse movement event */
	static uint8_t tmp[MOUSE_REPORT_COUNT] = {0};
	tmp[MOUSE_X_REPORT_IDX] = 10; // Move right
	k_msgq_put(&mouse_msgq, tmp, K_NO_WAIT);
}

static void button3_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	LOG_INF("*** BUTTON3 INTERRUPT TRIGGERED! pins=0x%x ***", pins);
	
	/* Send mouse movement event */
	static uint8_t tmp[MOUSE_REPORT_COUNT] = {0};
	tmp[MOUSE_Y_REPORT_IDX] = 10; // Move down
	k_msgq_put(&mouse_msgq, tmp, K_NO_WAIT);
}

static void mouse_iface_ready(const struct device *dev, const bool ready)
{
	LOG_INF("HID device %s interface is %s",
		dev->name, ready ? "ready" : "not ready");
	mouse_ready = ready;
}

static int mouse_get_report(const struct device *dev,
			 const uint8_t type, const uint8_t id, const uint16_t len,
			 uint8_t *const buf)
{
	LOG_WRN("Get Report not implemented, Type %u ID %u", type, id);

	return 0;
}

struct hid_device_ops mouse_ops = {
	.iface_ready = mouse_iface_ready,
	.get_report = mouse_get_report,
};

int main(void)
{
	struct usbd_context *sample_usbd;
	const struct device *hid_dev;
	int ret;

	LOG_INF("HID Mouse application started");

	/* Check button GPIO devices */
	if (!gpio_is_ready_dt(&button0)) {
		LOG_ERR("Button0 device is not ready");
	} else {
		LOG_INF("Button0 device is ready");
	}
	if (!gpio_is_ready_dt(&button1)) {
		LOG_ERR("Button1 device is not ready");
	} else {
		LOG_INF("Button1 device is ready");
	}

	if (!gpio_is_ready_dt(&led0)) {
		LOG_ERR("LED device %s is not ready", led0.port->name);
		return 0;
	}

	ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT);
	if (ret != 0) {
		LOG_ERR("Failed to configure the LED pin, error: %d", ret);
		return 0;
	}

	LOG_INF("LED configured successfully");

	/* Configure GPIO interrupts for buttons */
	LOG_INF("Configuring GPIO interrupts...");
	
	// Configure button0 as input with pull-up
	ret = gpio_pin_configure_dt(&button0, GPIO_INPUT | GPIO_PULL_UP);
	if (ret != 0) {
		LOG_ERR("Failed to configure button0, error: %d", ret);
		return ret;
	}
	
	// Configure button1 as input with pull-up
	ret = gpio_pin_configure_dt(&button1, GPIO_INPUT | GPIO_PULL_UP);
	if (ret != 0) {
		LOG_ERR("Failed to configure button1, error: %d", ret);
		return ret;
	}
	
	// Configure button0 interrupt (active low, trigger on both edges)
	ret = gpio_pin_interrupt_configure_dt(&button0, GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		LOG_ERR("Failed to configure button0 interrupt, error: %d", ret);
		return ret;
	}
	
	// Configure button1 interrupt (active low, trigger on both edges)
	ret = gpio_pin_interrupt_configure_dt(&button1, GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		LOG_ERR("Failed to configure button1 interrupt, error: %d", ret);
		return ret;
	}
	
	// Configure button2 as input with pull-up
	ret = gpio_pin_configure_dt(&button2, GPIO_INPUT | GPIO_PULL_UP);
	if (ret != 0) {
		LOG_ERR("Failed to configure button2, error: %d", ret);
		return ret;
	}
	
	// Configure button3 as input with pull-up
	ret = gpio_pin_configure_dt(&button3, GPIO_INPUT | GPIO_PULL_UP);
	if (ret != 0) {
		LOG_ERR("Failed to configure button3, error: %d", ret);
		return ret;
	}
	
	// Configure button2 interrupt
	ret = gpio_pin_interrupt_configure_dt(&button2, GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		LOG_ERR("Failed to configure button2 interrupt, error: %d", ret);
		return ret;
	}
	
	// Configure button3 interrupt
	ret = gpio_pin_interrupt_configure_dt(&button3, GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		LOG_ERR("Failed to configure button3 interrupt, error: %d", ret);
		return ret;
	}
	
	// Initialize and add callbacks
	gpio_init_callback(&button0_cb_data, button0_pressed, BIT(button0.pin));
	gpio_add_callback(button0.port, &button0_cb_data);
	
	gpio_init_callback(&button1_cb_data, button1_pressed, BIT(button1.pin));
	gpio_add_callback(button1.port, &button1_cb_data);
	
	gpio_init_callback(&button2_cb_data, button2_pressed, BIT(button2.pin));
	gpio_add_callback(button2.port, &button2_cb_data);
	
	gpio_init_callback(&button3_cb_data, button3_pressed, BIT(button3.pin));
	gpio_add_callback(button3.port, &button3_cb_data);
	
	LOG_INF("GPIO interrupts configured successfully");

	/* Check if GPIOTE interrupt is enabled in NVIC */
	// LOG_INF("Checking GPIOTE interrupt status...");
	// LOG_INF("IRQ 105 enabled: %d", irq_is_enabled(105));

	hid_dev = DEVICE_DT_GET_ONE(zephyr_hid_device);
	if (!device_is_ready(hid_dev)) {
		LOG_ERR("HID Device is not ready");
		return -EIO;
	}

	ret = hid_device_register(hid_dev,
				  hid_report_desc, sizeof(hid_report_desc),
				  &mouse_ops);
	if (ret != 0) {
		LOG_ERR("Failed to register HID Device, %d", ret);
		return ret;
	}

	sample_usbd = sample_usbd_init_device(NULL);
	if (sample_usbd == NULL) {
		LOG_ERR("Failed to initialize USB device");
		return -ENODEV;
	}

	ret = usbd_enable(sample_usbd);
	if (ret != 0) {
		LOG_ERR("Failed to enable device support");
		return ret;
	}

	LOG_DBG("USB device support enabled");

	while (true) {
		UDC_STATIC_BUF_DEFINE(report, MOUSE_REPORT_COUNT);

		ret = k_msgq_get(&mouse_msgq, &report, K_FOREVER);
		if (ret != 0) {
			LOG_ERR("Failed to get message from queue, error: %d", ret);
			continue;
		}

		if (!mouse_ready) {
			LOG_INF("USB HID device is not ready");
			continue;
		}

		ret = hid_device_submit_report(hid_dev, MOUSE_REPORT_COUNT, report);
		if (ret) {
			LOG_ERR("HID submit report error, %d", ret);
		} else {
			/* Toggle LED on sent report */
			(void)gpio_pin_toggle(led0.port, led0.pin);
		}
	}

	return 0;
}
