// AMF3 deserializer (dCommon/AMFDeserialize.cpp): the format of client-sent property-behaviour
// game messages. Reads one AMF3 value from the input bitstream the way GameMessages do, then
// re-serializes it with AmfSerialize. AMFDeserialize throws std::invalid_argument on a malformed
// marker / oversized string or array, and the real caller (GameMessages::HandleControlBehaviors)
// wraps Read() in catch (...), so an exception is a rejection, not a finding.
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include "AMFDeserialize.h"
#include "AmfSerialize.h"
#include "Amf3.h"
#include "BitStream.h"
#include "fuzz_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	FuzzInitLogger();
	RakNet::BitStream bs(const_cast<unsigned char*>(data), static_cast<unsigned int>(size), false);
	AMFDeserialize reader;
	try {
		auto value = reader.Read(bs);
		if (value) {
			RakNet::BitStream out;
			out.Write<AMFBaseValue&>(*value);
		}
	} catch (...) {  // GameMessages::HandleControlBehaviors, the caller, catches everything
		return 0;
	}
	return 0;
}
