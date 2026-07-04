import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import time as time_
from esphome.const import CONF_ID, CONF_TIME_ID

CODEOWNERS = ["@jarda"]
DEPENDENCIES = ["time"]
MULTI_CONF = False

zalejvatko_ns = cg.esphome_ns.namespace("zalejvatko")
ZalejvatkoComponent = zalejvatko_ns.class_("ZalejvatkoComponent", cg.PollingComponent)

CONF_ZALEJVATKO_ID = "zalejvatko_id"
CONF_CHANNEL = "channel"

CONF_VALVE_ENABLE_PIN = "valve_enable_pin"
CONF_VALVE_SIGNAL_PIN = "valve_signal_pin"
CONF_ADDRESS_PINS = "address_pins"
CONF_PUMP_PIN = "pump_pin"
CONF_ML_PER_SEC = "ml_per_sec"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ZalejvatkoComponent),
        cv.Required(CONF_TIME_ID): cv.use_id(time_.RealTimeClock),
        cv.Required(CONF_VALVE_ENABLE_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_VALVE_SIGNAL_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_ADDRESS_PINS): cv.All(
            cv.ensure_list(pins.gpio_output_pin_schema), cv.Length(min=4, max=4)
        ),
        cv.Required(CONF_PUMP_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_ML_PER_SEC, default=4.0): cv.positive_float,
    }
).extend(cv.polling_component_schema("1s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    time_var = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_var))

    enable_pin = await cg.gpio_pin_expression(config[CONF_VALVE_ENABLE_PIN])
    cg.add(var.set_valve_enable_pin(enable_pin))

    signal_pin = await cg.gpio_pin_expression(config[CONF_VALVE_SIGNAL_PIN])
    cg.add(var.set_valve_signal_pin(signal_pin))

    for i, pin_conf in enumerate(config[CONF_ADDRESS_PINS]):
        pin = await cg.gpio_pin_expression(pin_conf)
        cg.add(var.set_address_pin(i, pin))

    pump_pin = await cg.gpio_pin_expression(config[CONF_PUMP_PIN])
    cg.add(var.set_pump_pin(pump_pin))

    cg.add(var.set_ml_per_sec(config[CONF_ML_PER_SEC]))


# spolecne schema pro vsechny sub-platformy (switch/number/text/button) - odkaz na hub + cislo kanalu
CHANNEL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZALEJVATKO_ID): cv.use_id(ZalejvatkoComponent),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=15),
    }
)
