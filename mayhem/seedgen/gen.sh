#!/usr/bin/env bash
# Regenerates the binary seeds under mayhem/*/testsuite (after mayhem/build.sh has built build/fuzz).
# Run in /mayhem; the output is committed, nothing here runs during the build or the fuzzing.
set -euo pipefail
cd "${SRC:-/mayhem}"
F=build/fuzz
GLM=/opt/toolchains/deps/glm-1.0.3/src
mkdir -p mayhem/fuzz-clientpackets/testsuite mayhem/fuzz-raw/testsuite mayhem/fuzz-chatfilter/testsuite \
	mayhem/fuzz-auth/testsuite mayhem/fuzz-chat/testsuite
clang++ -fsanitize=address,undefined,fuzzer-no-link -std=c++20 -D_GLIBCXX_USE_CXX11_ABI=0 -D_GLIBCXX_USE_CXX17_ABI=0 -include mayhem/clang_compat.h \
	-Idmayhem_unused -IdCommon -IdCommon/dClient -IdCommon/dEnums -IdNet -IdChatServer -IdZoneManager \
	-isystem thirdparty/raknet/Source -isystem "$GLM" -isystem thirdparty/magic_enum/include/magic_enum \
	-isystem thirdparty/libbcrypt/include -IdDatabase/GameDatabase -IdDatabase/GameDatabase/ITables \
	mayhem/seedgen/gen_seeds.cpp -Wl,--start-group "$(find $F -name libdNet.a -print -quit)" "$(find $F -name libdCommon.a -print -quit)" \
	"$(find $F -name libraknet.a -print -quit)" "$(find $F -name libtinyxml2.a -print -quit)" -Wl,--end-group \
	-lz -lpthread -ldl -o /tmp/gen_seeds
/tmp/gen_seeds mayhem

# fuzz-controlbehaviors: upstream's ControlBehaviors test bitstreams, cut to the AMF array, plus the command.
mkdir -p mayhem/fuzz-controlbehaviors/testsuite
clang++ -fsanitize=address,undefined,fuzzer-no-link -std=c++20 -D_GLIBCXX_USE_CXX11_ABI=0 -D_GLIBCXX_USE_CXX17_ABI=0 -include mayhem/clang_compat.h \
	-IdCommon -IdCommon/dEnums -IdNet -isystem thirdparty/raknet/Source -isystem "$GLM" -isystem thirdparty/magic_enum/include/magic_enum \
	mayhem/seedgen/gen_cb_seeds.cpp -Wl,--start-group "$(find $F -name libdCommon.a -print -quit)" \
	"$(find $F -name libraknet.a -print -quit)" "$(find $F -name libtinyxml2.a -print -quit)" -Wl,--end-group \
	-lz -lpthread -ldl -o /tmp/gen_cb_seeds
TB=tests/dGameTests/dGameMessagesTests/TestBitStreams
for c in addStrip removeStrip mergeStrips splitStrip updateStripUI addAction migrateActions rearrangeStrip \
	removeActions updateAction rename add; do
	/tmp/gen_cb_seeds "$TB/$c" "$c" "mayhem/fuzz-controlbehaviors/testsuite/$c.amf"
done
for c in moveToInventory remove; do  # same arguments as "add" (BehaviorID + BehaviorIndex)
	/tmp/gen_cb_seeds "$TB/add" "$c" "mayhem/fuzz-controlbehaviors/testsuite/$c.amf"
done
