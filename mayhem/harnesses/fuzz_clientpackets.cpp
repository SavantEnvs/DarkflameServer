// World-server client packet deserializers (dNet/ClientPackets.cpp): the chat message, position
// update and chat-moderation request a connected client sends. The input is the raw packet as
// RakNet delivers it (header bytes included; the handlers skip the header themselves).
#include <cstddef>
#include <cstdint>
#include <vector>
#include "dCommonVars.h"
#include "ClientPackets.h"
#include "PositionUpdate.h"
#include "RakNetTypes.h"
#include "fuzz_common.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	FuzzInitLogger();
	std::vector<unsigned char> buf(data, data + size);
	Packet packet{};
	packet.data = buf.data();
	packet.length = static_cast<unsigned int>(size);
	packet.bitSize = static_cast<BitSize_t>(size * 8);
	auto chat = ClientPackets::HandleChatMessage(&packet);
	(void)chat;
	auto pos = ClientPackets::HandleClientPositionUpdate(&packet);
	(void)pos;
	auto mod = ClientPackets::HandleChatModerationRequest(&packet);
	(void)mod;
	return 0;
}
