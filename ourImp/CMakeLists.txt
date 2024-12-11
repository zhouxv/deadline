

#############################################
#               Build OKVS                  #
#############################################


set(SRCS "test.cpp")

add_library(OurImp ${SRCS})

target_link_libraries(OurImp PUBLIC volePSI batchPIR)

if(MSVC)
    target_compile_options(OurImp PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/std:c++${VOLE_PSI_STD_VER}>)
    #target_compile_options(libOTe PRIVATE -openmp:experimental)
else()
    target_compile_options(OurImp PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++${VOLE_PSI_STD_VER}>)
endif()