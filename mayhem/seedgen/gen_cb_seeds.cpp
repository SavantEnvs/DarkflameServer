// fuzz-controlbehaviors seeds: upstream's ControlBehaviors test bitstreams
// (tests/dGameTests/dGameMessagesTests/TestBitStreams/<command>) are 1 KiB dumps holding one AMF3
// array followed by unrelated bytes. Keep the bytes AMFDeserialize consumes, then append the
// command as HandleControlBehaviors reads it: a uint32_t length and the command bytes.
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "AMFDeserialize.h"
#include "Amf3.h"
#include "BitStream.h"
#include "Game.h"
#include "Logger.h"

namespace Game { Logger* logger = nullptr; }  // the servers define it in their main files

int main(int argc, char** argv) {
	if (argc < 4) { std::fprintf(stderr, "usage: %s <in> <command> <out>\n", argv[0]); return 2; }
	Game::logger = new Logger("/dev/null", false, false);
	std::ifstream in(argv[1], std::ios::binary);
	std::vector<unsigned char> data((std::istreambuf_iterator<char>(in)), {});
	RakNet::BitStream bs(data.data(), data.size(), false);
	AMFDeserialize reader;
	auto value = reader.Read(bs);
	if (!value || value->GetValueType() != eAmf::Array) { std::fprintf(stderr, "%s: not an AMF array\n", argv[1]); return 1; }
	const size_t used = (bs.GetReadOffset() + 7) / 8;
	RakNet::BitStream out;
	out.Write(reinterpret_cast<const char*>(data.data()), static_cast<unsigned int>(used));  // raw bytes
	const std::string command = argv[2];
	out.Write<uint32_t>(command.size());
	for (const char c : command) out.Write<unsigned char>(c);
	std::ofstream file(argv[3], std::ios::binary);
	file.write(reinterpret_cast<const char*>(out.GetData()), out.GetNumberOfBytesUsed());
	if (!file) { std::fprintf(stderr, "%s: write failed\n", argv[3]); return 1; }
	return 0;
}
