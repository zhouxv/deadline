

#############################################
#               Build volePSI_Tests            #
#############################################


set(SRCS
	"Paxos_Tests.cpp"
    "FileBase_Tests.cpp"
    "UnitTests.cpp"
	)

add_library(volePSI_Tests ${SRCS})

target_link_libraries(volePSI_Tests volePSI)


if(MSVC)
    target_compile_options(volePSI_Tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/std:c++${VOLE_PSI_STD_VER}>)
    #target_compile_options(libOTe PRIVATE -openmp:experimental)
else()
    target_compile_options(volePSI_Tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++${VOLE_PSI_STD_VER}>)
endif()