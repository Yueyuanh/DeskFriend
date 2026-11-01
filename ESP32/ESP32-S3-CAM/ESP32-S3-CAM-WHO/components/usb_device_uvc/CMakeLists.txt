if(${IDF_VERSION_MAJOR} LESS 6)
    set(usb_requires usb)
else()
    set(usb_requires esp_hw_support)
endif()

idf_component_register(SRCS usb_device_uvc.c
                    INCLUDE_DIRS "include"
                    REQUIRES ${usb_requires} esp_timer)

idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)

target_include_directories(${tusb_lib} PUBLIC "${COMPONENT_DIR}/tusb")
# PRIVATE instead of upstream's PUBLIC: INTERFACE_SOURCES made every consumer
# (including this component) compile usb_descriptors.c again, and PlatformIO's
# SCons build then maps those copies onto the same object path, aborting with
# "Two environments with different actions were specified for the same target".
# The descriptor file only needs to live in the TinyUSB archive.
target_sources(${tusb_lib} PRIVATE "${COMPONENT_DIR}/tusb/usb_descriptors.c")

include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
