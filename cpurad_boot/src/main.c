/**
******************************************************************************
* Copyright (c) 2025 Nordic Semiconductor ASA
* Filename : main.c
* Date(DD-MM-YYYY)        Modification        Name
* ----------------        ------------        ----
*    19-09-2025             Created            JF
*******************************************************************************
*/

/* Includes ------------------------------------------------------------------*/
#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/arch/arm/arch.h>
#include <arm_cleanup.h>
#include <nrf_cleanup.h>
#include <zephyr/cache.h>
#include <zephyr/drivers/timer/system_timer.h>
#include <zephyr/logging/log.h>
#include <hal/nrf_gpio.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_hid.h>
#include <sample_usbd.h>
/* Macro----------------------------------------------------------------------*/
#define LOG_MODULE_NAME boot
LOG_MODULE_REGISTER(LOG_MODULE_NAME);


#define MRAM_NODE               DT_NODELABEL(mram1x)
#define APP_PARTITION_NODE      DT_NODELABEL(cpurad_app_partition)
#define TARGET_IMAGE_ADDRESS    (DT_REG_ADDR(MRAM_NODE) + DT_REG_ADDR(APP_PARTITION_NODE))

#define TEST_PIN_1 NRF_GPIO_PIN_MAP(9,0)/* MC : pin 9.0 */
#define TEST_PIN_2 NRF_GPIO_PIN_MAP(0,8)/* MC : pin 0.8 */



/* Private typedef -----------------------------------------------------------*/
typedef struct arm_vector_table_s 
{
	uint32_t msp;	       /* Initial stack pointer */
	uint32_t reset_vector; /* Reset handler address */
}arm_vector_table_t;

/* Private function prototypes------------------------------------------------*/
static void __attribute__((noreturn)) jump_to_image(uint32_t image_addr);

/* Global variables ----------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Macro ---------------------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

#ifdef CONFIG_USB_DEVICE_STACK_NEXT
// static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
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

// K_MSGQ_DEFINE(mouse_msgq, MOUSE_REPORT_COUNT, 2, 1);
static bool mouse_ready;
struct usbd_context *sample_usbd;
const struct device *hid_dev;
// static void input_cb(struct input_event *evt, void *user_data)
// {
// 	static uint8_t tmp[MOUSE_REPORT_COUNT];

// 	ARG_UNUSED(user_data);

// 	switch (evt->code) {
// 	case INPUT_KEY_0:
// 		WRITE_BIT(tmp[MOUSE_BTN_REPORT_IDX], MOUSE_BTN_LEFT, evt->value);
// 		break;
// 	case INPUT_KEY_1:
// 		WRITE_BIT(tmp[MOUSE_BTN_REPORT_IDX], MOUSE_BTN_RIGHT, evt->value);
// 		break;
// 	case INPUT_KEY_2:
// 		if (evt->value) {
// 			tmp[MOUSE_X_REPORT_IDX] += 10U;
// 		}

// 		break;
// 	case INPUT_KEY_3:
// 		if (evt->value) {
// 			tmp[MOUSE_Y_REPORT_IDX] += 10U;
// 		}

// 		break;
// 	default:
// 		printk("Unrecognized input code %u value %d\n",
// 			evt->code, evt->value);
// 		return;
// 	}

// 	if (k_msgq_put(&mouse_msgq, tmp, K_NO_WAIT) != 0) {
// 		printk("Failed to put new input event\n");
// 	}

// 	tmp[MOUSE_X_REPORT_IDX] = 0U;
// 	tmp[MOUSE_Y_REPORT_IDX] = 0U;

// }

// INPUT_CALLBACK_DEFINE(NULL, input_cb, NULL);

static void mouse_iface_ready(const struct device *dev, const bool ready)
{
	printk("HID device %s interface is %s\n",
		dev->name, ready ? "ready" : "not ready");
	mouse_ready = ready;
}

static int mouse_get_report(const struct device *dev,
			 const uint8_t type, const uint8_t id, const uint16_t len,
			 uint8_t *const buf)
{
	printk("Get Report not implemented, Type %u ID %u\n", type, id);

	return 0;
}

struct hid_device_ops mouse_ops = {
	.iface_ready = mouse_iface_ready,
	.get_report = mouse_get_report,
};

int hsusb_init(void)
{

	int ret;

	// if (!gpio_is_ready_dt(&led0)) {
	// 	printk("LED device %s is not ready\n", led0.port->name);
	// 	return 0;
	// }

	// ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT);
	// if (ret != 0) {
	// 	printk("Failed to configure the LED pin, error: %d\n", ret);
	// 	return 0;
	// }

	hid_dev = DEVICE_DT_GET_ONE(zephyr_hid_device);
	if (!device_is_ready(hid_dev)) {
		printk("HID Device is not ready\n");
		return -EIO;
	}

	ret = hid_device_register(hid_dev,
				  hid_report_desc, sizeof(hid_report_desc),
				  &mouse_ops);
	if (ret != 0) {
		printk("Failed to register HID Device, %d\n", ret);
		return ret;
	}

	sample_usbd = sample_usbd_init_device(NULL);
	if (sample_usbd == NULL) {
		printk("Failed to initialize USB device\n");
		return -ENODEV;
	}

	ret = usbd_enable(sample_usbd);
	if (ret != 0) {
		printk("Failed to enable device support\n");
		return ret;
	}

	printk("USB device support enabled\n");

	return 0;
}
#endif

int main(void)
{
    LOG_PRINTK("****************************************\n");
    LOG_PRINTK("rad boot started\r\n");
    LOG_PRINTK("****************************************\n");
    nrf_gpio_cfg_output(TEST_PIN_1);
    nrf_gpio_pin_set(TEST_PIN_1); /* MC : set pin high to indicate bootloader is running */
    
    /* Configure P0.08 as input and check its level */
    nrf_gpio_cfg_input(TEST_PIN_2, NRF_GPIO_PIN_NOPULL);
    uint32_t pin_level = nrf_gpio_pin_read(TEST_PIN_2);
    if (pin_level) {
        LOG_PRINTK("P0.08 is HIGH\n");
    } else {
        LOG_PRINTK("P0.08 is LOW\n");
    }
    
    //customer code put here
#ifdef CONFIG_USB_DEVICE_STACK_NEXT
    hsusb_init();
    k_msleep(5000);
#endif
    //end of customer code

    jump_to_image(TARGET_IMAGE_ADDRESS);

    return 0;
}

/* Overrride functions -------------------------------------------------------*/

/* Private Functions ---------------------------------------------------------*/

/**
 * @brief Jump to another image at specified address
 *
 * This function performs a clean jump to another firmware image by:
 * 1. Disabling all interrupts
 * 2. Clearing NVIC pending interrupts
 * 3. Setting the vector table offset register (VTOR)
 * 4. Loading the new stack pointer and reset vector
 * 5. Jumping to the new image
 *
 * @param image_addr Address of the target image
 */
static void __attribute__((noreturn)) jump_to_image(uint32_t image_addr)
{
	arm_vector_table_t *vt = (arm_vector_table_t *)image_addr;

	LOG_PRINTK("Jumping to image at address 0x%08x\n", image_addr);
	LOG_PRINTK("Stack pointer: 0x%08x\n", vt->msp);
	LOG_PRINTK("Reset vector: 0x%08x\n", vt->reset_vector);

#ifdef CONFIG_USB_DEVICE_STACK_NEXT
	/* Properly shutdown USB before jumping */
	if (sample_usbd != NULL) {
		LOG_PRINTK("Shutting down USB\n");
		usbd_disable(sample_usbd);
		usbd_shutdown(sample_usbd);
		k_msleep(1000); /* Wait for USB to fully shutdown */
	}
#endif
    
    nrf_cleanup_peripheral();
    cleanup_arm_nvic(); /* cleanup NVIC registers */
    
    /* Additional delay for VBUS detection service to stabilize */
    k_msleep(500);
    
    LOG_PRINTK("GPIO and peripheral cleanup completed\n");

    /* Flush and disable instruction/data caches before chain-loading the application */
    (void)sys_cache_instr_flush_all();
    (void)sys_cache_data_flush_all();
    sys_cache_instr_disable();
    sys_cache_data_disable();

	z_arm_clear_arm_mpu_config();

#if defined(CONFIG_BUILTIN_STACK_GUARD) && defined(CONFIG_CPU_CORTEX_M_HAS_SPLIM)
	/* Reset stack limit registers */
	__set_PSPLIM(0);
	__set_MSPLIM(0);
#endif

	__set_MSP(vt->msp);
    __set_CONTROL(0x00); /* application will configures core on its own */
	__ISB();

	/* Jump to the new image reset vector */
	((void (*)(void))vt->reset_vector)();

	/* Should never reach here */
	CODE_UNREACHABLE;
}

/* Macro ---------------------------------------------------------------------*/

