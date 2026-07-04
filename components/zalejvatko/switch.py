import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from .. import zalejvatko_ns, CHANNEL_SCHEMA, CONF_ZALEJVATKO_ID, CONF_CHANNEL

ZalejvatkoChannelSwitch = zalejvatko_ns.class_(
    "ZalejvatkoChannelSwitch", switch.Switch, cg.Component
)

CONFIG_SCHEMA = switch.switch_schema(ZalejvatkoChannelSwitch).extend(CHANNEL_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config["id"])
    await switch.register_switch(var, config)
    await cg.register_component(var, config)

    hub = await cg.get_variable(config[CONF_ZALEJVATKO_ID])
    cg.add(var.set_parent(hub))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(hub.register_enable_switch(config[CONF_CHANNEL], var))
