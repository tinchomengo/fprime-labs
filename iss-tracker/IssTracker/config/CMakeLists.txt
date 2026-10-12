####
# IssTracker configuration overrides
#
# Replaces framework configuration files with the project's copies. Only
# FW_CMD_STRING_MAX_SIZE differs from the F´ v4.2.2 defaults (see FpConstants.fpp).
####
register_fprime_config(
        project_config
    CONFIGURATION_OVERRIDES
        "${CMAKE_CURRENT_LIST_DIR}/FpConstants.fpp"
    INTERFACE
)
