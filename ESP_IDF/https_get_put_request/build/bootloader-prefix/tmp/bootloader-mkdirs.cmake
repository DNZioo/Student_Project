# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/esp/esp-idf/v5.3/esp-idf/components/bootloader/subproject"
  "D:/Academic year5/Semester1/Student_Project/ESP_IDF/https_get_put_request/build/bootloader"
  "D:/Academic year5/Semester1/Student_Project/ESP_IDF/https_get_put_request/build/bootloader-prefix"
  "D:/Academic year5/Semester1/Student_Project/ESP_IDF/https_get_put_request/build/bootloader-prefix/tmp"
  "D:/Academic year5/Semester1/Student_Project/ESP_IDF/https_get_put_request/build/bootloader-prefix/src/bootloader-stamp"
  "D:/Academic year5/Semester1/Student_Project/ESP_IDF/https_get_put_request/build/bootloader-prefix/src"
  "D:/Academic year5/Semester1/Student_Project/ESP_IDF/https_get_put_request/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Academic year5/Semester1/Student_Project/ESP_IDF/https_get_put_request/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/Academic year5/Semester1/Student_Project/ESP_IDF/https_get_put_request/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
