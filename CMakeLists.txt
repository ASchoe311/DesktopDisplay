cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr)
project(desktop_display)

target_sources(app PRIVATE
        src/main.c
        src/drivers/lcd/lcd.c
        src/serialdata.c
)

target_include_directories(app PRIVATE src)