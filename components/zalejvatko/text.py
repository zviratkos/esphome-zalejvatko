import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text
from esphome.const import CONF_MODE

from . import zalejvatko_ns, CHANNEL_SCHEMA, CONF_ZALEJVATKO_ID, CONF_CHANNEL

ZalejvatkoChannelScheduleText = zalejvatko_ns.class_(
    "ZalejvatkoChannelScheduleText", text.Text, cg.Component
)

CONFIG_SCHEMA = (
    text.text_schema(
        ZalejvatkoChannelScheduleText,
        icon="mdi:clock-outline",
    )
    .extend(
        {
            # vychozi rezim "text" (ne "password"), takze to netreba psat v YAML rucne
            cv.Optional(CONF_MODE, default="text"): cv.enum(
                text.TEXT_MODES, upper=True
            ),
        }
    )
    .extend(CHANNEL_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config["id"])
    # max delka odpovida MAX_SCHEDULE_STRLEN v C++ (48, tj. 47 znaku + terminator)
    await text.register_text(var, config, min_length=0, max_length=47)
    await cg.register_component(var, config)

    hub = await cg.get_variable(config[CONF_ZALEJVATKO_ID])
    cg.add(var.set_parent(hub))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(hub.register_schedule_text(config[CONF_CHANNEL], var))

