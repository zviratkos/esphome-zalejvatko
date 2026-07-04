import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from . import zalejvatko_ns, CHANNEL_SCHEMA, CONF_ZALEJVATKO_ID, CONF_CHANNEL

ZalejvatkoChannelWaterNowButton = zalejvatko_ns.class_(
    "ZalejvatkoChannelWaterNowButton", button.Button, cg.Component
)

CONFIG_SCHEMA = button.button_schema(
    ZalejvatkoChannelWaterNowButton,
    icon="mdi:water-pump",
).extend(CHANNEL_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config["id"])
    await button.register_button(var, config)
    await cg.register_component(var, config)

    hub = await cg.get_variable(config[CONF_ZALEJVATKO_ID])
    cg.add(var.set_parent(hub))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(hub.register_water_now_button(config[CONF_CHANNEL], var))
