// LXFML brick-model XML (dCommon/Lxfml.cpp, LxfmlBugged.cpp): the decompressed content of a
// client-uploaded model. Runs every public entry point the server uses on one document.
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include "Lxfml.h"
#include "NiPoint3.h"
#include "fuzz_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	FuzzInitLogger();
	// Every server caller hands these functions a view of a std::string (the decompressed SD0
	// buffer), which is NUL-terminated; LxfmlBugged.cpp relies on that (Parse without a length).
	const std::string doc(reinterpret_cast<const char*>(data), size);
	const std::string_view sv(doc);
	auto split = Lxfml::Split(sv);
	(void)split;
	auto norm = Lxfml::NormalizePosition(sv);
	(void)norm;
	auto first = Lxfml::NormalizePositionOnlyFirstPart(sv);
	(void)first;
	auto after = Lxfml::NormalizePositionAfterFirstPart(sv, NiPoint3(1.0f, 2.0f, 3.0f));
	(void)after;
	return 0;
}
