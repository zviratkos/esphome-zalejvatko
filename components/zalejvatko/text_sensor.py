import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import zalejvatko_ns, CHANNEL_SCHEMA, CONF_ZALEJVATKO_ID, CONF_CHANNEL

ZalejvatkoChannelLastWateredTextSensor = zalejvatko_ns.class_(
    "ZalejvatkoChannelLastWateredTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = text_sensor.text_sensor_schema(
    ZalejvatkoChannelLastWateredTextSensor,
    icon="mdi:watering-can",
).extend(CHANNEL_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config["id"])
    await text_sensor.register_text_sensor(var, config)
    await cg.register_component(var, config)

    hub = await cg.get_variable(config[CONF_ZALEJVATKO_ID])
    cg.add(var.set_parent(hub))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(hub.register_last_watered_sensor(config[CONF_CHANNEL], var))
