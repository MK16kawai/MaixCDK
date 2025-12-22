set(compile_from_src 0)

if(CONFIG_LIBDATACHANNEL_COMPILE_FROM_SOURCE)
    set(compile_from_src 1)
endif()

if (compile_from_src)
    set(libdatachannel_version_str "${CONFIG_LIBDATACHANNEL_VERSION_MAJOR}.${CONFIG_LIBDATACHANNEL_VERSION_MINOR}.${CONFIG_LIBDATACHANNEL_VERSION_PATCH}")
    set(srcs_path "${DL_EXTRACTED_PATH}/libdatachannel_srcs/libdatachannel-${libdatachannel_version_str}")
    list(APPEND ADD_INCLUDE "${srcs_path}/include" ".")
    list(APPEND ADD_PRIVATE_INCLUDE "${srcs_path}/include" "${srcs_path}/src")

    if(PLATFORM_MAIXCAM)
    list(APPEND ADD_SRCS  "private/atomic.cpp")
    endif()

    aux_source_directory("${srcs_path}/src" ADD_SRCS)
    aux_source_directory("${srcs_path}/src/impl" ADD_SRCS)
    list(APPEND ADD_REQUIREMENTS openssl json libjuice libsrtp plog usrsctp cpp-httplib)
    list(APPEND ADD_DEFINITIONS_PRIVATE -DJUICE_STATIC -DRTC_ENABLE_MEDIA=1 -DRTC_ENABLE_WEBSOCKET=1 -DRTC_EXPORTS -DRTC_SYSTEM_JUICE=0 -DRTC_SYSTEM_SRTP=0 -DUSE_GNUTLS=0 -DUSE_NICE=0 -D_GNU_SOURCE -Ddatachannel_EXPORTS -DNDEBUG -w)

    register_component(DYNAMIC)
else()
    list(APPEND ADD_INCLUDE "include" ".")
    list(APPEND ADD_REQUIREMENTS json cpp-httplib)
    if (PLATFORM_MAIXCAM)
        list(APPEND ADD_DYNAMIC_LIB "lib/maixcam/libdatachannel.so")
    elseif(PLATFORM_MAIXCAM2)
        list(APPEND ADD_DYNAMIC_LIB "lib/maixcam2/libdatachannel.so")
    else()
        list(APPEND ADD_REQUIREMENTS libdatachannel)
    endif()

    register_component()
endif()

