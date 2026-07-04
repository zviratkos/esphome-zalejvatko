import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_UNIT_OF_MEASUREMENT, CONF_ICON

from .. import zalejvatko_ns, CHANNEL_SCHEMA, CONF_ZALEJVATKO_ID, CONF_CHANNEL

ZalejvatkoChannelDoseNumber = zalejvatko_ns.class_(
    "ZalejvatkoChannelDoseNumber", number.Number, cg.Component
)

CONFIG_SCHEMA = number.number_schema(
    ZalejvatkoChannelDoseNumber,
    unit_of_measurement="ml",
    icon="mdi:water",
).extend(CHANNEL_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config["id"])
    await number.register_number(var, config, min_value=0, max_value=2000, step=5)
    await cg.register_component(var, config)

    hub = await cg.get_variable(config[CONF_ZALEJVATKO_ID])
    cg.add(var.set_parent(hub))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(hub.register_dose_number(config[CONF_CHANNEL], var))
