from esphome import pins, automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from esphome.const import CONF_ID, CONF_INTERRUPT_PIN

CONF_SDN_PIN = "sdn_pin"

CONF_CHANNEL = "channel"
CONF_CMD = "cmd"
CONF_TARGET = "target"
CONF_LEVEL = "level"

DEPENDENCIES = ["spi"]

temperbridge_ns = cg.esphome_ns.namespace("temperbridge")
TemperBridge = temperbridge_ns.class_(
    "TemperBridgeComponent", cg.Component, spi.SPIDevice
)

temperbridge_simple_command_ns = temperbridge_ns.enum("SimpleCommand", is_class=True)

SIMPLE_COMMANDS = {
    "flat": temperbridge_simple_command_ns.PRESET_FLAT,
    "mode_1": temperbridge_simple_command_ns.PRESET_MODE1,
    "mode_2": temperbridge_simple_command_ns.PRESET_MODE2,
    "mode_3": temperbridge_simple_command_ns.PRESET_MODE3,
    "mode_4": temperbridge_simple_command_ns.PRESET_MODE4,
    "save_preset_mode1": temperbridge_simple_command_ns.SAVE_PRESET_MODE1,
    "save_preset_mode2": temperbridge_simple_command_ns.SAVE_PRESET_MODE2,
    "save_preset_mode3": temperbridge_simple_command_ns.SAVE_PRESET_MODE3,
    "save_preset_mode4": temperbridge_simple_command_ns.SAVE_PRESET_MODE4,
    "stop": temperbridge_simple_command_ns.STOP,
    "massage_mode_1": temperbridge_simple_command_ns.MASSAGE_PRESET_MODE1,
    "massage_mode_2": temperbridge_simple_command_ns.MASSAGE_PRESET_MODE2,
    "massage_mode_3": temperbridge_simple_command_ns.MASSAGE_PRESET_MODE3,
    "massage_mode_4": temperbridge_simple_command_ns.MASSAGE_PRESET_MODE4,
}

validate_simple_command = cv.enum(SIMPLE_COMMANDS, lower=True)

temperbridge_position_command_ns = temperbridge_ns.enum(
    "PositionCommand", is_class=True
)

POSITION_COMMANDS = {
    "raise_head": temperbridge_position_command_ns.RAISE_HEAD,
    "raise_legs": temperbridge_position_command_ns.RAISE_LEGS,
    "lower_head": temperbridge_position_command_ns.LOWER_HEAD,
    "lower_legs": temperbridge_position_command_ns.LOWER_LEGS,
}

validate_position_command = cv.enum(POSITION_COMMANDS, lower=True)

temperbridge_massage_target_enum = temperbridge_ns.enum("MassageTarget", is_class=True)

MASSAGE_TARGET = {
    "head": temperbridge_massage_target_enum.HEAD,
    "legs": temperbridge_massage_target_enum.LEGS,
    "lumbar": temperbridge_massage_target_enum.LUMBAR,
}

validate_massage_target = cv.enum(MASSAGE_TARGET, lower=True)

ExecuteSimpleCommandAction = temperbridge_ns.class_(
    "ExecuteSimpleCommandAction", automation.Action
)

PositionCommandAction = temperbridge_ns.class_(
    "PositionCommandAction", automation.Action
)

SetMassageIntensityAction = temperbridge_ns.class_(
    "SetMassageIntensityAction", automation.Action
)

SetChannelAction = temperbridge_ns.class_("SetChannelAction", automation.Action)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TemperBridge),
            cv.Required(CONF_SDN_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_INTERRUPT_PIN): cv.All(
                pins.internal_gpio_input_pin_schema
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    interrupt_pin = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
    cg.add(var.set_interrupt_pin(interrupt_pin))

    sdn_pin = await cg.gpio_pin_expression(config[CONF_SDN_PIN])
    cg.add(var.set_sdn_pin(sdn_pin))


@automation.register_action(
    "temperbridge.position_command_2",
    PositionCommandAction,
    maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(TemperBridge),
            cv.Required(CONF_CMD): cv.templatable(validate_position_command),
        },
    ),
)
async def temperbridge_start_position_command_to_code(
    config, action_id, template_arg, args
):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_cmd(config[CONF_CMD]))
    return var


@automation.register_action(
    "temperbridge.execute_simple_command",
    ExecuteSimpleCommandAction,
    maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(TemperBridge),
            cv.Required(CONF_CMD): cv.templatable(validate_simple_command),
        },
    ),
)
async def temperbridge_execute_simple_command_to_code(
    config, action_id, template_arg, args
):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_cmd(config[CONF_CMD]))
    return var


validate_channel = cv.All(cv.int_range(min=0, max=9999))


@automation.register_action(
    "temperbridge.set_channel",
    SetChannelAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(TemperBridge),
            cv.Required(CONF_CHANNEL): cv.templatable(validate_channel),
        },
        key=CONF_CHANNEL,
    ),
)
async def temperbridge_set_channel_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_CHANNEL], args, cg.uint16)
    cg.add(var.set_channel(template_))
    return var


validate_massage_level = cv.All(cv.int_range(min=0, max=10))


@automation.register_action(
    "temperbridge.set_massage_intensity",
    SetMassageIntensityAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(TemperBridge),
            cv.Required(CONF_TARGET): cv.templatable(validate_massage_target),
            cv.Required(CONF_LEVEL): cv.templatable(validate_massage_level),
        }
    ),
)
async def temperbridge_set_massage_intensity_to_code(
    config, action_id, template_arg, args
):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_TARGET], args, validate_massage_target)
    cg.add(var.set_target(template_))

    template_ = await cg.templatable(config[CONF_LEVEL], args, cg.uint8)
    cg.add(var.set_level(template_))
    return var
