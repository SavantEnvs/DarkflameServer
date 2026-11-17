// LDF "key=type:value" parser (dCommon/LDFFormat.cpp): level-object configs, settings and
// game-message payloads. Parses the input, then re-renders the value as a string and as the
// network encoding, exercising every typed LDFData specialization.
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include "LDFFormat.h"
#include "BitStream.h"
#include "fuzz_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	FuzzInitLogger();
	// Callers pass views of std::strings (NUL-terminated); DataFromString LOGs them with %s.
	const std::string text(reinterpret_cast<const char*>(data), size);
	auto ldf = LDFBaseData::DataFromString(text);
	if (ldf) {
		auto s = ldf->GetString();
		(void)s;
		RakNet::BitStream out;
		ldf->WriteToPacket(out);
	}
	return 0;
}
