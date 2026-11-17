// Terrain .raw parser (dZoneManager/Raw.cpp): client terrain files (versions 30-32: chunks,
// height/colour/scene maps, flairs) the world server loads for a zone, then the terrain mesh it
// generates from them.
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include "Raw.h"
#include "fuzz_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	FuzzInitLogger();
	std::istringstream ss(std::string(reinterpret_cast<const char*>(data), size), std::ios::binary);
	Raw::Raw raw;
	if (Raw::ReadRaw(ss, raw)) {
		Raw::TerrainMesh mesh;
		Raw::GenerateTerrainMesh(raw, mesh);
	}
	return 0;
}
