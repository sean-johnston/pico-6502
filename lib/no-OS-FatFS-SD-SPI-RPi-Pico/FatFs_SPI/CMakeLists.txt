add_library(FatFs_SPI INTERFACE)

if($ENV{PICO_PLATFORM}X STREQUAL "rp2350X")
message(STATUS "PICO 2")
target_sources(FatFs_SPI INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ffsystem.c
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ffunicode.c
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/sd_spi.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/demo_logging.c
#    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/hw_config.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/spi.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/sd_card.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/crc.c
    ${CMAKE_CURRENT_LIST_DIR}/src/glue.c
    ${CMAKE_CURRENT_LIST_DIR}/src/f_util.c
    ${CMAKE_CURRENT_LIST_DIR}/src/ff_stdio.c
    ${CMAKE_CURRENT_LIST_DIR}/src/my_debug.c
    ${CMAKE_CURRENT_LIST_DIR}/src/rtc-rp2350.c
)
target_link_libraries(FatFs_SPI INTERFACE
#       hardware_rtc
        hardware_spi
        hardware_dma
        pico_stdlib
)
else()
message(STATUS "PICO 1")
target_sources(FatFs_SPI INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ffsystem.c
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ffunicode.c
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/sd_spi.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/demo_logging.c
#    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/hw_config.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/spi.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/sd_card.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/crc.c
    ${CMAKE_CURRENT_LIST_DIR}/src/glue.c
    ${CMAKE_CURRENT_LIST_DIR}/src/f_util.c
    ${CMAKE_CURRENT_LIST_DIR}/src/ff_stdio.c
    ${CMAKE_CURRENT_LIST_DIR}/src/my_debug.c
    ${CMAKE_CURRENT_LIST_DIR}/src/rtc.c
)
target_link_libraries(FatFs_SPI INTERFACE
        hardware_rtc
        hardware_spi
        hardware_dma
        pico_stdlib
)
endif()


target_include_directories(FatFs_SPI INTERFACE
    ff15/source
    sd_driver
    include
)
#target_link_libraries(FatFs_SPI INTERFACE
#       hardware_rtc
#        hardware_spi
#        hardware_dma
#        pico_stdlib
#)
