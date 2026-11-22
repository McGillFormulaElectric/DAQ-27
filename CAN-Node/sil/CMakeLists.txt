cmake_minimum_required(VERSION 3.20)
project(mfe26_sil LANGUAGES CXX)

option(SIL_SANITIZE "Build SIL executables with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(SIL_COVERAGE "Build SIL executables with gcov-compatible coverage instrumentation" OFF)

function(sil_enable_sanitizers target)
    if(SIL_SANITIZE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        else()
            message(FATAL_ERROR "SIL_SANITIZE requires a GCC- or Clang-compatible host compiler")
        endif()
    endif()
endfunction()

function(sil_enable_coverage target)
    if(SIL_COVERAGE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${target} PRIVATE --coverage)
            target_link_options(${target} PRIVATE --coverage)
        else()
            message(FATAL_ERROR "SIL_COVERAGE requires a GCC- or Clang-compatible host compiler")
        endif()
    endif()
endfunction()

enable_testing()

add_executable(sil_ads7066_model_tests
    test_ads7066_model.cpp
    ads7066_model.cpp
    ads7066_bus.cpp
    ads7066_fault_overlay.cpp)
target_compile_features(sil_ads7066_model_tests PRIVATE cxx_std_17)
target_compile_options(sil_ads7066_model_tests PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(sil_ads7066_model_tests PRIVATE .)
add_test(NAME sil_ads7066_model_tests COMMAND sil_ads7066_model_tests)
sil_enable_sanitizers(sil_ads7066_model_tests)
sil_enable_coverage(sil_ads7066_model_tests)

add_executable(sil_adc_tests
    test_adc.cpp
    fakes/fake_hal.cpp
    fakes/virtual_can.cpp
    ads7066_model.cpp
    ads7066_fault_overlay.cpp
    "../Drivers/MFE26-G4-Drivers/Src/HAL Drivers/IOPin.cpp"
    "../Drivers/MFE26-G4-Drivers/Src/HAL Drivers/SPIDriver.cpp"
    "../Drivers/MFE26-G4-Drivers/Src/HAL Drivers/SPIDevice.cpp"
    "../Drivers/MFE26-G4-Drivers/Src/Device Drivers/ADS7066.cpp")

target_compile_features(sil_adc_tests PRIVATE cxx_std_17)
target_compile_options(sil_adc_tests PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(sil_adc_tests PRIVATE
    fakes
    "../Drivers/MFE26-G4-Drivers/Inc/HAL Drivers"
    "../Drivers/MFE26-G4-Drivers/Inc/Device Drivers")

add_test(NAME sil_adc_tests COMMAND sil_adc_tests)
sil_enable_sanitizers(sil_adc_tests)
sil_enable_coverage(sil_adc_tests)

add_executable(sil_can_tests
    test_can.cpp fakes/fake_fdcan.cpp fakes/virtual_can.cpp
    "../Drivers/MFE26-G4-Drivers/Src/HAL Drivers/CANMessage.cpp"
    "../Drivers/MFE26-G4-Drivers/Src/HAL Drivers/CANDriver.cpp")
target_compile_features(sil_can_tests PRIVATE cxx_std_17)
target_compile_options(sil_can_tests PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(sil_can_tests PRIVATE fakes "../Drivers/MFE26-G4-Drivers/Inc/HAL Drivers")
add_test(NAME sil_can_tests COMMAND sil_can_tests)
sil_enable_sanitizers(sil_can_tests)
sil_enable_coverage(sil_can_tests)

add_executable(sil_application_tests
    test_application.cpp fakes/fake_hal.cpp fakes/virtual_can.cpp
    ads7066_model.cpp ads7066_fault_overlay.cpp
    "../Core/Src/BoardManager.cpp" "../Core/Src/Wrapper.cpp"
    "../Drivers/MFE26-G4-Drivers/Src/HAL Drivers/IOPin.cpp"
    "../Drivers/MFE26-G4-Drivers/Src/HAL Drivers/SPIDriver.cpp"
    "../Drivers/MFE26-G4-Drivers/Src/HAL Drivers/SPIDevice.cpp"
    "../Drivers/MFE26-G4-Drivers/Src/HAL Drivers/CANMessage.cpp"
    "../Drivers/MFE26-G4-Drivers/Src/HAL Drivers/CANDriver.cpp"
    "../Drivers/MFE26-G4-Drivers/Src/Device Drivers/ADS7066.cpp")
target_compile_features(sil_application_tests PRIVATE cxx_std_17)
target_compile_options(sil_application_tests PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(sil_application_tests PRIVATE fakes ../Core/Inc
    "../Drivers/MFE26-G4-Drivers/Inc/HAL Drivers" "../Drivers/MFE26-G4-Drivers/Inc/Device Drivers")
add_test(NAME sil_application_tests COMMAND sil_application_tests)
sil_enable_sanitizers(sil_application_tests)
sil_enable_coverage(sil_application_tests)
