// SD0 container (dCommon/Sd0.cpp + ZCompression.cpp): chunked zlib wrapper of the LXFML brick
// models a client uploads (GameMessages BBB model save path). Parses the input as an SD0 stream,
// decompresses it and hands the result to the LXFML splitter, as the server does; also
// re-compresses the input with Sd0::FromData and round-trips that.
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include "Sd0.h"
#include "Lxfml.h"
#include "fuzz_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	FuzzInitLogger();
	std::stringstream ss(std::string(reinterpret_cast<const char*>(data), size));
	Sd0 sd0(ss);
	const std::string uncompressed = sd0.GetAsStringUncompressed();
	if (!uncompressed.empty()) {
		auto parts = Lxfml::Split(uncompressed);
		(void)parts;
	}
	std::stringstream empty;
	Sd0 repacked(empty);
	repacked.FromData(data, size);
	const std::string back = repacked.GetAsStringUncompressed();
	(void)back;
	return 0;
}
