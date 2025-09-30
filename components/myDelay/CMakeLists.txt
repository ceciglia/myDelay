set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES audio_sal audio_hal esp_dispatcher esp_peripherals display_service)

set(COMPONENT_SRCS "LFO.c" "myDelay.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()