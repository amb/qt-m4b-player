# CMake also respects SOURCE_DATE_EPOCH for reproducible release builds.
string(TIMESTAMP PLAYER_BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S UTC" UTC)
file(CONFIGURE OUTPUT "${OUTPUT_FILE}" CONTENT
"#pragma once
#define PLAYER_BUILD_TIMESTAMP \"@PLAYER_BUILD_TIMESTAMP@\"
" @ONLY)
