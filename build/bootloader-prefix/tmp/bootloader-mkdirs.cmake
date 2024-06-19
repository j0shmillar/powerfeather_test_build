# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/Users/joshmillar/esp/esp-idf/components/bootloader/subproject"
  "/Users/joshmillar/Desktop/phd/PowerFeather/supply_and_battery_info/build/bootloader"
  "/Users/joshmillar/Desktop/phd/PowerFeather/supply_and_battery_info/build/bootloader-prefix"
  "/Users/joshmillar/Desktop/phd/PowerFeather/supply_and_battery_info/build/bootloader-prefix/tmp"
  "/Users/joshmillar/Desktop/phd/PowerFeather/supply_and_battery_info/build/bootloader-prefix/src/bootloader-stamp"
  "/Users/joshmillar/Desktop/phd/PowerFeather/supply_and_battery_info/build/bootloader-prefix/src"
  "/Users/joshmillar/Desktop/phd/PowerFeather/supply_and_battery_info/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/joshmillar/Desktop/phd/PowerFeather/supply_and_battery_info/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/joshmillar/Desktop/phd/PowerFeather/supply_and_battery_info/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
