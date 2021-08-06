set(include_dirs core core/sysdep core/utils 
    components/bootstrap
    components/data-model
    components/devinfo
    components/diag
    components/logpost
    components/ntp
    components/ota
    components/shadow
    components/subdev
    components/task)

file(GLOB c_sdk_srcs
    "core/*.c"
    "core/utils/*.c"
    "core/sysdep/*.c"

    "components/bootstrap/*.c"
    "components/data-model/*.c"
    "components/devinfo/*.c"
    "components/diag/*.c"
    "components/logpost/*.c"
    "components/ntp/*.c"
    "components/ota/*.c"
    "components/shadow/*.c"
    "components/subdev/*.c"
    "components/task/*.c"

    "external/*.c"

    "portfiles/aiot_port/*.c")

idf_component_register(SRCS ${c_sdk_srcs}
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES json mbedtls)
