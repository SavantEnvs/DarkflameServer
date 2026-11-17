#!/usr/bin/env bash
#
# mayhem/build.sh — builds DarkflameServer's fuzz harnesses (sanitized) and its gtest suite (normal
# flags). Runs in /mayhem as `mayhem`; re-runnable and offline: glm and googletest (CMake
# FetchContent upstream) come from /opt/toolchains/deps (see mayhem/Dockerfile); the MariaDB
# connector is the in-tree submodule; zlib/OpenSSL are the base image's apt -dev packages.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

GLM_SRC=/opt/toolchains/deps/glm-1.0.3/src
GTEST_SRC=/opt/toolchains/deps/googletest-1.12.1/src
OFFLINE=(-DFETCHCONTENT_FULLY_DISCONNECTED=ON
         -DFETCHCONTENT_SOURCE_DIR_GLM="$GLM_SRC"
         -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$GTEST_SRC")

# Both build trees live under build/, which upstream's .gitignore already covers (SPEC §6.2 item 16).
FUZZ_BUILD="$SRC/build/fuzz"
TEST_BUILD="$SRC/build/test"
OUT="$SRC/build/bin"
mkdir -p "$OUT"

# 1) The project with sanitizers + coverage instrumentation, so the fuzzed dCommon code is
#    instrumented (libFuzzer's -fsanitize=fuzzer-no-link on the library, the engine at link time).
FUZZ_CFLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link"
# clang 19 rejects upstream's clang-only GeneralUtils branch as ordered; mayhem/clang_compat.h
# (C++ only, declarations only) supplies the conforming order without editing upstream.
COMPAT="-include $SRC/mayhem/clang_compat.h"
cmake -S . -B "$FUZZ_BUILD" -DCMAKE_BUILD_TYPE=None \
	-DCMAKE_C_FLAGS="$FUZZ_CFLAGS" -DCMAKE_CXX_FLAGS="$FUZZ_CFLAGS $COMPAT" "${OFFLINE[@]}"
cmake --build "$FUZZ_BUILD" -j"$MAYHEM_JOBS" --target dCommon dNet dZoneManager dChatServer dChatFilter dWeb raknet tinyxml2 bcrypt MD5 mongoose \
	dPropertyBehaviors

INCS=(-I"$SRC/dCommon" -I"$SRC/dCommon/dClient" -I"$SRC/dCommon/dEnums" -I"$SRC/dNet" -I"$SRC/dZoneManager"
      -I"$SRC/dDatabase/GameDatabase" -I"$SRC/dDatabase/GameDatabase/ITables" -I"$SRC/dDatabase/CDClientDatabase"
      -isystem "$SRC/thirdparty/raknet/Source" -isystem "$SRC/thirdparty/tinyxml2" -isystem "$GLM_SRC"
      -isystem "$SRC/thirdparty/magic_enum/include/magic_enum" -isystem "$SRC/thirdparty/nlohmann"
      -isystem "$SRC/thirdparty/SQLite" -isystem "$SRC/thirdparty/libbcrypt/include"
      -I"$SRC/dDatabase" -I"$SRC/dChatServer" -I"$SRC/dChatFilter" -I"$SRC/dWeb" -I"$SRC/mayhem/harnesses"
      -I"$SRC/dGame/dPropertyBehaviors" -I"$SRC/dGame/dPropertyBehaviors/ControlBehaviorMessages")
DEFS=(-D_GLIBCXX_USE_CXX11_ABI=0 -D_GLIBCXX_USE_CXX17_ABI=0 -std=c++20)
LIBS=("$FUZZ_BUILD/libdNet.a" "$FUZZ_BUILD/libdCommon.a" "$FUZZ_BUILD/libraknet.a" "$FUZZ_BUILD/libtinyxml2.a"
      "$FUZZ_BUILD/libbcrypt.a" -lz -lpthread -ldl)

# The chat and auth harnesses also link the database layer (the game database interface, whose test
# implementation the harnesses subclass), the chat server's handlers and the web server it registers routes with.
CHAT_LIBS=(libdChatServer.a libdChatFilter.a libdWeb.a libmongoose.a)
DB_LIBS=(libdDatabase.a libdDatabaseGame.a libdDatabaseCDClient.a libsqlite3.a libMD5.a libmariadbcpp-static.a libmariadbclient.a)

# Resolve library paths wherever this CMake layout put them.
RESOLVED=()
for l in "${LIBS[@]}"; do
	case "$l" in
		/*) f=$(find "$FUZZ_BUILD" -name "$(basename "$l")" -print -quit); [ -n "$f" ] || { echo "missing $l" >&2; exit 1; }; RESOLVED+=("$f") ;;
		*) RESOLVED+=("$l") ;;
	esac
done

find_lib() { find "$FUZZ_BUILD" -name "$1" -print -quit; }
resolve() { local f; for l in "$@"; do f=$(find_lib "$l"); [ -n "$f" ] || { echo "missing $l" >&2; exit 1; }; printf '%s\n' "$f"; done; }
mapfile -t DB_RESOLVED < <(resolve "${DB_LIBS[@]}")
mapfile -t CHAT_RESOLVED < <(resolve "${CHAT_LIBS[@]}")
SYSLIBS=(-lssl -lcrypto)

HFLAGS="$FUZZ_CFLAGS"
# dZoneManager is a CMake OBJECT library (no archive): link the terrain reader's object directly.
RAW_OBJ="$FUZZ_BUILD/dZoneManager/CMakeFiles/dZoneManager.dir/Raw.cpp.o"
[ -f "$RAW_OBJ" ] || { echo "missing $RAW_OBJ" >&2; exit 1; }
# dPropertyBehaviors is an OBJECT library too (compiled with upstream's own includes and PCH): link
# only its ControlBehaviorMessages objects, the parsers of the client's property-behaviour arguments,
# which need dCommon and tinyxml2 alone (the rest of the library needs the game's components).
CBM_DIR="$FUZZ_BUILD/dGame/dPropertyBehaviors/CMakeFiles/dPropertyBehaviors.dir/ControlBehaviorMessages"
mapfile -t CBM_OBJS < <(find "$CBM_DIR" -name '*.cpp.o' | sort)
[ "${#CBM_OBJS[@]}" -gt 0 ] || { echo "no ControlBehaviorMessages objects in $CBM_DIR" >&2; exit 1; }
$CXX $HFLAGS $COMPAT "${DEFS[@]}" "${INCS[@]}" -c "$SRC/mayhem/harnesses/fuzz_globals.cpp" -o "$OUT/fuzz_globals.o"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$OUT/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/standalone_main.o"

for h in fuzz_amf fuzz_sd0 fuzz_lxfml fuzz_ldf fuzz_clientpackets fuzz_raw fuzz_chatfilter fuzz_auth fuzz_chat fuzz_controlbehaviors; do
	extra=(); post=("${SYSLIBS[@]}")
	case "$h" in
		fuzz_raw) extra=("$RAW_OBJ") ;;
		fuzz_chatfilter) extra=("${CHAT_RESOLVED[@]}" "${DB_RESOLVED[@]}") ;;
		fuzz_auth) extra=("${DB_RESOLVED[@]}") ;;
		fuzz_chat) extra=("${CHAT_RESOLVED[@]}" "${DB_RESOLVED[@]}") ;;
		fuzz_controlbehaviors) extra=("${CBM_OBJS[@]}") ;;
	esac
	$CXX $HFLAGS $COMPAT "${DEFS[@]}" "${INCS[@]}" -c "$SRC/mayhem/harnesses/$h.cpp" -o "$OUT/$h.o"
	$CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OUT/$h.o" "$OUT/fuzz_globals.o" "$OUT/lsan_off.o" \
		-Wl,--start-group "${extra[@]}" "${RESOLVED[@]}" -Wl,--end-group "${post[@]}" -o "$OUT/$h"
	$CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$OUT/standalone_main.o" "$OUT/$h.o" "$OUT/fuzz_globals.o" "$OUT/lsan_off.o" \
		-Wl,--start-group "${extra[@]}" "${RESOLVED[@]}" -Wl,--end-group "${post[@]}" -o "$OUT/$h-standalone"
done

# 2) Upstream's gtest suite (dCommonTests, dGameTests) with the project's normal flags.
cmake -S . -B "$TEST_BUILD" -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COMPAT $COVERAGE_FLAGS" "${OFFLINE[@]}"
cmake --build "$TEST_BUILD" -j"$MAYHEM_JOBS" --target dCommonTests dGameTests
