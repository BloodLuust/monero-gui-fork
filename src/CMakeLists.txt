add_subdirectory(QR-Code-scanner)
add_subdirectory(openpgp)
add_subdirectory(zxcvbn-c)

qt6_add_resources(RESOURCES ../qml.qrc)

# Compile source files (.h/.cpp)
file(GLOB SOURCE_FILES
	"*.h"
	"*.cpp"
    "main/*.h"
    "main/*.cpp"
    "libwalletqt/WalletManager.cpp"
    "libwalletqt/WalletListenerImpl.cpp"
    "libwalletqt/Wallet.cpp"
    "libwalletqt/PassphraseHelper.cpp"
    "libwalletqt/PendingTransaction.cpp"
    "libwalletqt/TransactionHistory.cpp"
    "libwalletqt/TransactionInfo.cpp"
    "libwalletqt/QRCodeImageProvider.cpp"
    "libwalletqt/AddressBook.cpp"
    "libwalletqt/Subaddress.cpp"
    "libwalletqt/SubaddressAccount.cpp"
    "libwalletqt/UnsignedTransaction.cpp"
    "libwalletqt/WalletManager.h"
    "libwalletqt/Wallet.h"
    "libwalletqt/PassphraseHelper.h"
    "libwalletqt/PendingTransaction.h"
    "libwalletqt/TransactionHistory.h"
    "libwalletqt/TransactionInfo.h"
    "libwalletqt/QRCodeImageProvider.h"
    "libwalletqt/Transfer.h"
    "libwalletqt/AddressBook.h"
    "libwalletqt/Subaddress.h"
    "libwalletqt/SubaddressAccount.h"
    "libwalletqt/UnsignedTransaction.h"
    "daemon/*.h"
    "daemon/*.cpp"
    "p2pool/*.h"
    "p2pool/*.cpp"
    "model/*.h"
    "model/*.cpp"
    "qt/*.h"
    "qt/*.cpp"
    "I2PManager.cpp"
    "I2PManager.h"
)
if(APPLE)
    list(APPEND SOURCE_FILES "qt/macoshelper.mm")
endif()

set(EXECUTABLE_FLAG)
if(MINGW)
    set(EXECUTABLE_FLAG WIN32)

    set(ICON ${PROJECT_SOURCE_DIR}/images/appicon.ico)
    set(ICON_RC ${CMAKE_CURRENT_BINARY_DIR}/icon.rc)
    set(ICON_RES ${CMAKE_CURRENT_BINARY_DIR}/icon.o)
    file(WRITE ${ICON_RC} "IDI_ICON1 ICON DISCARDABLE \"${ICON}\"")
    find_program(Qt6_WINDRES_EXECUTABLE NAMES windres x86_64-w64-mingw32-windres REQUIRED CMAKE_FIND_ROOT_PATH_BOTH)
    add_custom_command(OUTPUT ${ICON_RES} COMMAND ${Qt6_WINDRES_EXECUTABLE} ${ICON_RC} ${ICON_RES} MAIN_DEPENDENCY ${ICON_RC})
    list(APPEND RESOURCES ${ICON_RES})
endif()

if(APPLE)
    set(ICON ${PROJECT_SOURCE_DIR}/images/appicon.icns)
    set_source_files_properties(${ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    list(APPEND RESOURCES ${ICON})
endif()

set(monero_wallet_gui_sources
    ${SOURCE_FILES}
    ${RESOURCES}
)

if(NOT ANDROID)
    add_executable(monero-wallet-gui ${EXECUTABLE_FLAG} ${monero_wallet_gui_sources})
else()
    add_library(monero-wallet-gui SHARED ${monero_wallet_gui_sources})
    set_target_properties(monero-wallet-gui PROPERTIES COMPILE_DEFINITIONS "ANDROID")
endif()

set_target_properties(monero-wallet-gui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/share/Info.plist"
)

# OpenGL
target_include_directories(monero-wallet-gui PUBLIC ${OPENGL_INCLUDE_DIR})
message(STATUS "OpenGL: include dir at ${OPENGL_INCLUDE_DIR}")
message(STATUS "OpenGL: libraries at ${OPENGL_LIBRARIES}")

target_include_directories(monero-wallet-gui PUBLIC ${Qt6Gui_PRIVATE_INCLUDE_DIRS})

target_include_directories(monero-wallet-gui PUBLIC
	${CMAKE_SOURCE_DIR}/monero/include
    ${CMAKE_SOURCE_DIR}/monero/src
    ${CMAKE_SOURCE_DIR}/monero/external/easylogging++
    ${CMAKE_SOURCE_DIR}/monero/contrib/epee/include
    ${CMAKE_SOURCE_DIR}/monero/external/qrcodegen
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/daemon
    ${CMAKE_CURRENT_SOURCE_DIR}/p2pool
	${CMAKE_CURRENT_SOURCE_DIR}/libwalletqt
	${CMAKE_CURRENT_SOURCE_DIR}/model
	${CMAKE_CURRENT_SOURCE_DIR}/QR-Code-scanner
	${CMAKE_CURRENT_SOURCE_DIR}/zxcvbn-c
    ${X11_INCLUDE_DIR}
)

target_compile_definitions(monero-wallet-gui
	PUBLIC
	${Qt6Widgets_DEFINITIONS}
    ${Qt6Qml_DEFINITIONS}
)

if(APPLE)
    if(NOT ICU_ROOT)
        execute_process(COMMAND brew --prefix icu4c OUTPUT_VARIABLE ICU_ROOT OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    find_package(ICU REQUIRED COMPONENTS data i18n uc)
    target_link_directories(monero-wallet-gui PRIVATE ${ICU_ROOT}/lib)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt6Widgets_EXECUTABLE_COMPILE_FLAGS}")

target_link_libraries(monero-wallet-gui
    epee
    common
    net
    wallet_api
    qrcodegen
    easylogging
    ${QT6_LIBRARIES}
    ${EXTRA_LIBRARIES}
    openpgp
    qrdecoder
    translations
    zxcvbn
)

if(X11_FOUND)
    target_link_libraries(monero-wallet-gui ${X11_LIBRARIES})
endif()

if(WITH_SCANNER)
    target_link_libraries(monero-wallet-gui qrscanner)
    if(LINUX AND NOT ANDROID)
        target_link_libraries(monero-wallet-gui
            jpeg
            v4l2
            v4lconvert
            rt
        )
    endif()
endif()

add_custom_command(TARGET monero-wallet-gui POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:daemon> $<TARGET_FILE_DIR:monero-wallet-gui>)

# Copy platform-specific i2pd binary
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # Windows 64-bit
        add_custom_command(TARGET monero-wallet-gui POST_BUILD 
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/external/i2pd/win64/i2pd.exe $<TARGET_FILE_DIR:monero-wallet-gui>)
    endif()
elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        # macOS ARM64 (Apple Silicon)
        add_custom_command(TARGET monero-wallet-gui POST_BUILD 
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/external/i2pd/osx-arm64/i2pd $<TARGET_FILE_DIR:monero-wallet-gui>)
    else()
        # macOS x86_64 (Intel)
        add_custom_command(TARGET monero-wallet-gui POST_BUILD 
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/external/i2pd/osx-x86_64/i2pd $<TARGET_FILE_DIR:monero-wallet-gui>)
    endif()
elseif(UNIX AND NOT APPLE)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        # Linux 64-bit
        add_custom_command(TARGET monero-wallet-gui POST_BUILD 
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/external/i2pd/linux64/i2pd $<TARGET_FILE_DIR:monero-wallet-gui>)
    endif()
elseif(ANDROID)
    if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
        # Android ARM64
        add_custom_command(TARGET monero-wallet-gui POST_BUILD 
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/external/i2pd/android-arm64/i2pd $<TARGET_FILE_DIR:monero-wallet-gui>)
    endif()
endif()

include(Deploy)

install(TARGETS monero-wallet-gui
    DESTINATION bin
)
