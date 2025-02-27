#define DT_DRV_COMPAT zmk_behavior_haptic

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zephyr/kernel.h> // for k_work, k_work_init_delayable, etc.

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

// Data struct (optional).
// We'll keep a delayed work here so we can turn off the motor after a pulse.
struct behavior_haptic_data {
    struct k_work_delayable release_work;
    const struct device *gpio_dev;
    gpio_pin_t pin;
    gpio_flags_t flags;
};

// Config struct (properties from devicetree).
struct behavior_haptic_config {
    uint32_t pulse_ms;
    // The phandle-array for gpios from the devicetree
    struct gpio_dt_spec motor;
};

// Forward declaration for our “release motor” function.
static void motor_release_work_handler(struct k_work *work);

// Our pressed function: set the GPIO high, schedule a delayed work to set it low.
static int on_haptic_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_haptic_data *data = dev->data;
    const struct behavior_haptic_config *cfg = dev->config;

    // Immediately set pin high
    gpio_pin_set(data->gpio_dev, data->pin, 1);

    // Schedule the delayed work that will turn it off
    // after pulse_ms
    k_work_reschedule(&data->release_work, K_MSEC(cfg->pulse_ms));

    // Return OPAQUE so no further processing is done for the key press
    return ZMK_BEHAVIOR_OPAQUE;
}

// Our released function does nothing special.
// We already scheduled the motor turn-off.
static int on_haptic_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    // If you prefer the haptic to end exactly on key release, you could turn off
    // the motor here instead. We'll rely on the scheduled pulse for this example.
    return ZMK_BEHAVIOR_OPAQUE;
}

// Called from on_haptic_binding_pressed -> scheduled work
static void motor_release_work_handler(struct k_work *work) {
    // Convert from k_work back to our data pointer
    struct k_work_delayable *dwork = CONTAINER_OF(work, struct k_work_delayable, work);
    struct behavior_haptic_data *data = CONTAINER_OF(dwork, struct behavior_haptic_data, release_work);

    // Turn off the motor
    gpio_pin_set(data->gpio_dev, data->pin, 0);
}

// Initialization function
static int behavior_haptic_init(const struct device *dev) {
    struct behavior_haptic_data *data = dev->data;
    const struct behavior_haptic_config *cfg = dev->config;

    // Get the GPIO device/pin from the config
    data->gpio_dev = device_get_binding(cfg->motor.port->name);
    data->pin = cfg->motor.pin;
    data->flags = cfg->motor.dt_flags;

    if (!data->gpio_dev) {
        LOG_ERR("Failed to get haptic motor GPIO device");
        return -ENODEV;
    }

    int ret = gpio_pin_configure(data->gpio_dev, data->pin, GPIO_OUTPUT_INACTIVE | data->flags);
    if (ret < 0) {
        LOG_ERR("Failed to configure haptic motor GPIO pin");
        return ret;
    }

    // Initialize the delayed work
    k_work_init_delayable(&data->release_work, motor_release_work_handler);

    return 0;
}

// Declare the driver API
static const struct behavior_driver_api behavior_haptic_driver_api = {
    .binding_pressed = on_haptic_binding_pressed,
    .binding_released = on_haptic_binding_released,
    // If you wanted to declare metadata for ZMK Studio, you could do so here.
    // .parameter_metadata = ...
    // .get_parameter_metadata = ...
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
};

// Setup the config/data for each instance. We’ll assume we only have one instance for now.
static struct behavior_haptic_data behavior_haptic_data_0;
static const struct behavior_haptic_config behavior_haptic_config_0 = {
    .pulse_ms = DT_INST_PROP(0, pulse_ms),
    .motor = GPIO_DT_SPEC_INST_GET(0, gpios),
};

// Finally, create the device instance for our driver
BEHAVIOR_DT_INST_DEFINE(0,
                        behavior_haptic_init,
                        NULL,
                        &behavior_haptic_data_0,
                        &behavior_haptic_config_0,
                        APPLICATION,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_haptic_driver_api);

#endif // DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
