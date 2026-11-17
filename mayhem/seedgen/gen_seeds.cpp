// Writes the hand-built seed corpora under mayhem/<target>/testsuite. Not part of build.sh: it is run
// once (mayhem/seedgen/gen.sh) and its output is committed, so the seeds are plain binary files.
// Every packet is built with the server's own BitStream types and WriteHeader, so the layouts follow
// the handlers that read them.
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include "BitStreamUtils.h"
#include "ClientPackets.h"
#include "AuthPackets.h"
#include "ChatPacketHandler.h"
#include "MessageType/Auth.h"
#include "MessageType/World.h"
#include "MessageType/Chat.h"
#include "MessageType/Server.h"
#include "eGameMasterLevel.h"
#include "dCommonVars.h"

using Bytes = std::vector<unsigned char>;

static Bytes Take(RakNet::BitStream& bs) {
	return Bytes(bs.GetData(), bs.GetData() + bs.GetNumberOfBytesUsed());
}

static void Put(const std::string& path, const Bytes& b) {
	std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

static void Pad(RakNet::BitStream& bs, size_t n) {
	for (size_t i = 0; i < n; ++i) bs.Write<uint8_t>(0);
}

// ---- world client packets (fuzz-clientpackets): the handlers skip the 8-byte header
static void Hdr(RakNet::BitStream& bs) { BitStreamUtils::WriteHeader(bs, ServiceType::WORLD, MessageType::World::VALIDATION); }

static Bytes ClientChat(const std::u16string& text, uint8_t channel) {
	RakNet::BitStream bs;
	Hdr(bs);
	bs.Write<uint8_t>(channel);
	bs.Write<uint16_t>(0);
	bs.Write<int32_t>(static_cast<int32_t>(text.size() + 1));
	bs.Write(text);
	bs.Write<char16_t>(0);
	return Take(bs);
}

static Bytes Position(bool velocity, bool angular, bool local, bool remote) {
	RakNet::BitStream bs;
	Hdr(bs);
	for (float f : {10.5f, 3.25f, -42.0f}) bs.Write(f);
	for (float f : {0.0f, 0.707f, 0.0f, 0.707f}) bs.Write(f);
	bs.Write<bool>(true);
	bs.Write<bool>(false);
	bs.Write<bool>(velocity);
	if (velocity) for (float f : {1.0f, 0.0f, -2.0f}) bs.Write(f);
	bs.Write<bool>(angular);
	if (angular) for (float f : {0.0f, 0.5f, 0.0f}) bs.Write(f);
	bs.Write<bool>(local);
	if (local) {
		bs.Write<uint64_t>(0x1122334455667788ULL);
		for (float f : {1.0f, 2.0f, 3.0f}) bs.Write(f);
		bs.Write<bool>(true);
		for (float f : {0.5f, 0.25f, 0.0f}) bs.Write(f);
	}
	bs.Write<bool>(remote);
	if (remote) {
		bs.Write(0.5f);
		bs.Write(-1.0f);
		bs.Write<bool>(true);
		bs.Write<bool>(false);
	}
	return Take(bs);
}

static Bytes Moderation(const std::string& receiver, const std::string& text, uint8_t level) {
	RakNet::BitStream bs;
	Hdr(bs);
	bs.Write<uint8_t>(level);
	bs.Write<uint8_t>(7);
	for (size_t i = 0; i < 42; ++i) bs.Write<uint16_t>(i < receiver.size() ? static_cast<uint8_t>(receiver[i]) : 0);
	bs.Write<uint16_t>(static_cast<uint16_t>(text.size()));
	for (char c : text) bs.Write<uint16_t>(static_cast<uint8_t>(c));
	return Take(bs);
}

// ---- terrain (fuzz-raw): a one-chunk terrain, written for versions 30, 31 and 32
static void W32(std::string& s, uint32_t v) { s.append(reinterpret_cast<const char*>(&v), 4); }
static void Wf(std::string& s, float v) { s.append(reinterpret_cast<const char*>(&v), 4); }
static void W16(std::string& s, uint16_t v) { s.append(reinterpret_cast<const char*>(&v), 2); }

static Bytes Terrain(uint16_t version, uint32_t dim, uint32_t numFlairs, bool meshes) {
	std::string s;
	W16(s, version);
	s.push_back(0); // dev
	W32(s, 1);      // numChunks
	W32(s, 1);      // numChunksWidth
	W32(s, 1);      // numChunksHeight
	W32(s, 0);      // chunk id
	W32(s, dim);    // width
	W32(s, dim);    // height
	Wf(s, 0.0f);    // offsetX
	Wf(s, 0.0f);    // offsetZ
	if (version < 32) W32(s, 2); // shader id
	for (uint32_t t = 1; t <= 4; ++t) W32(s, t); // textures
	Wf(s, 4.0f);    // scale factor
	for (uint32_t i = 0; i < dim * dim; ++i) Wf(s, static_cast<float>(i % dim) * 0.5f); // heightmap
	const uint32_t colorRes = version >= 32 ? dim - 1 : dim - 1;
	if (version >= 32) {
		W32(s, colorRes);
		s.append(static_cast<size_t>(colorRes) * colorRes * 4, '\x80');
		W32(s, 4); s.append("DDS ", 4); // light map
	} else {
		s.append(static_cast<size_t>(dim) * dim * 4, '\x80'); // legacy colour map
	}
	const uint32_t texRes = 4;
	W32(s, texRes);
	s.append(static_cast<size_t>(texRes) * texRes * 4, '\x40');
	if (version >= 32) {
		s.push_back(1); // texture settings (a byte)
		W32(s, 4); s.append("DDS ", 4); // blend map
	}
	W32(s, numFlairs);
	for (uint32_t i = 0; i < numFlairs; ++i) {
		W32(s, i + 1); Wf(s, 1.0f);
		for (float f : {1.0f, 2.0f, 3.0f, 0.0f, 0.5f, 0.0f}) Wf(s, f);
		s.append("\x10\x20\x30\x00", 4);
	}
	if (version >= 32) {
		for (uint32_t i = 0; i < colorRes * colorRes; ++i) s.push_back(static_cast<char>(i % 3));
		W32(s, meshes ? 3 : 0); // vertSize
		if (meshes) {
			for (uint16_t v : {0, 1, 2}) W16(s, v);
			for (int i = 0; i < 16; ++i) W16(s, 3);
			for (int i = 0; i < 16; ++i) { W16(s, 3); for (uint16_t v : {0, 1, 2}) W16(s, v); }
		}
	} else {
		for (uint32_t i = 0; i < (colorRes + 1) * (colorRes + 1); ++i) s.push_back(static_cast<char>(i % 3));
	}
	return Bytes(s.begin(), s.end());
}

// ---- auth server (fuzz-auth)
static Bytes Handshake() {
	RakNet::BitStream bs;
	BitStreamUtils::WriteHeader(bs, ServiceType::COMMON, MessageType::Server::VERSION_CONFIRM);
	bs.Write<uint32_t>(171022);
	bs.Write<uint32_t>(0);
	bs.Write(ServiceType::CLIENT);
	bs.Write<uint16_t>(0);
	bs.Write<uint32_t>(1234);
	bs.Write<uint16_t>(2001);
	Pad(bs, 33);
	return Take(bs);
}

static Bytes Login(const std::string& user, const std::string& password) {
	RakNet::BitStream bs;
	BitStreamUtils::WriteHeader(bs, ServiceType::AUTH, MessageType::Auth::LOGIN_REQUEST);
	bs.Write(LUWString(user));
	bs.Write(LUWString(password, 41));
	bs.Write(LanguageCodeID::en_US);
	bs.Write(ClientOS::WINDOWS);
	bs.Write(LUWString(std::string("2048MB RAM"), 256));
	bs.Write(LUWString(std::string("Fuzz GPU"), 128));
	bs.Write<uint32_t>(8); bs.Write<uint32_t>(586); bs.Write<uint16_t>(6); bs.Write<uint16_t>(1);
	bs.Write<uint32_t>(148); bs.Write<uint32_t>(10); bs.Write<uint32_t>(0); bs.Write<uint32_t>(19045); bs.Write<uint32_t>(2);
	return Take(bs);
}

// ---- chat server (fuzz-chat): each packet is framed with a 2-byte little-endian length
static void Chat(RakNet::BitStream& bs, MessageType::Chat id) { BitStreamUtils::WriteHeader(bs, ServiceType::CHAT, id); }

static void Frame(Bytes& out, const Bytes& pkt) {
	out.push_back(pkt.size() & 0xff);
	out.push_back(pkt.size() >> 8);
	out.insert(out.end(), pkt.begin(), pkt.end());
}

static Bytes ChatLogin(uint64_t id, const std::string& name, uint8_t gm = 0, uint64_t muteExpire = 0) {
	RakNet::BitStream bs;
	Chat(bs, MessageType::Chat::LOGIN_SESSION_NOTIFY);
	bs.Write<uint64_t>(id);
	bs.Write<uint32_t>(static_cast<uint32_t>(name.size()));
	bs.WriteAlignedBytes(reinterpret_cast<const unsigned char*>(name.data()), static_cast<unsigned>(name.size()));
	LWOZONEID zone(1000, 0, 0);
	bs.Write(zone);
	bs.Write<time_t>(static_cast<time_t>(muteExpire));
	bs.Write<uint8_t>(gm);
	return Take(bs);
}

static Bytes GeneralChat(uint64_t id, eChatChannel channel, const std::u16string& text) {
	RakNet::BitStream bs;
	Chat(bs, MessageType::Chat::GENERAL_CHAT_MESSAGE);
	bs.Write<uint64_t>(id);
	bs.Write<uint32_t>(0);
	bs.Write(channel);
	bs.Write<uint32_t>(static_cast<uint32_t>(text.size() + 1));
	Pad(bs, 77);
	bs.Write(LUWString(text, static_cast<uint32_t>(text.size() + 1)));
	return Take(bs);
}

static Bytes PrivateChat(uint64_t id, const std::string& to, const std::u16string& text) {
	RakNet::BitStream bs;
	Chat(bs, MessageType::Chat::PRIVATE_CHAT_MESSAGE);
	bs.Write<uint64_t>(id);
	bs.Write<uint32_t>(0);
	bs.Write(eChatChannel::PRIVATE_CHAT);
	bs.Write<uint32_t>(static_cast<uint32_t>(text.size() + 1));
	Pad(bs, 77);
	bs.Write(LUWString(to));
	bs.Write<uint16_t>(0);
	bs.Write(LUWString(text, static_cast<uint32_t>(text.size() + 1)));
	return Take(bs);
}

static Bytes WithName(MessageType::Chat m, uint64_t id, const std::string& name, bool trailer, char flag = 0) {
	RakNet::BitStream bs;
	Chat(bs, m);
	bs.Write<uint64_t>(id);
	bs.Write<uint32_t>(0);
	bs.Write(LUWString(name));
	if (trailer) bs.Write<char>(flag);
	return Take(bs);
}

static Bytes IdOnly(MessageType::Chat m, uint64_t id) {
	RakNet::BitStream bs;
	Chat(bs, m);
	bs.Write<uint64_t>(id);
	return Take(bs);
}

static Bytes InviteResponse(uint64_t id, char declined, uint64_t leader) {
	RakNet::BitStream bs;
	Chat(bs, MessageType::Chat::TEAM_INVITE_RESPONSE);
	bs.Write<uint64_t>(id);
	bs.Write<uint32_t>(0);
	bs.Write<char>(declined);
	bs.Write<uint64_t>(leader);
	return Take(bs);
}

static Bytes LootOption(uint64_t id, uint32_t option) {
	RakNet::BitStream bs;
	Chat(bs, MessageType::Chat::TEAM_SET_LOOT);
	bs.Write<uint64_t>(id);
	bs.Write<uint32_t>(0);
	bs.Write<uint32_t>(option);
	return Take(bs);
}

int main(int argc, char** argv) {
	const std::string root = argc > 1 ? argv[1] : "mayhem";
	const auto dir = [&](const char* t) { return root + "/" + t + "/testsuite/"; };

	Put(dir("fuzz-clientpackets") + "chat_say.pkt", ClientChat(u"Hello world", 4));
	Put(dir("fuzz-clientpackets") + "chat_team.pkt", ClientChat(u"the quick brown fox", 8));
	Put(dir("fuzz-clientpackets") + "position_plain.pkt", Position(false, false, false, false));
	Put(dir("fuzz-clientpackets") + "position_velocity.pkt", Position(true, true, false, false));
	Put(dir("fuzz-clientpackets") + "position_full.pkt", Position(true, true, true, true));
	Put(dir("fuzz-clientpackets") + "moderation_plain.pkt", Moderation("Bob", "hello there", 1));
	Put(dir("fuzz-clientpackets") + "moderation_gm.pkt", Moderation("[GM]Admin", "i need help", 2));

	Put(dir("fuzz-raw") + "v32_chunk.raw", Terrain(32, 5, 0, false));
	Put(dir("fuzz-raw") + "v32_flairs_mesh.raw", Terrain(32, 5, 2, true));
	Put(dir("fuzz-raw") + "v31_chunk.raw", Terrain(31, 5, 1, false));
	Put(dir("fuzz-raw") + "v30_chunk.raw", Terrain(30, 5, 0, false));
	{
		std::string s;
		W16(s, 32); s.push_back(1); // dev terrain: header only
		Put(dir("fuzz-raw") + "v32_dev.raw", Bytes(s.begin(), s.end()));
	}

	Put(dir("fuzz-chatfilter") + "clean.txt", Bytes{'h','e','l','l','o',' ','w','o','r','l','d'});
	{
		const std::string t[] = {"the quick brown fox jumps over the lazy dog", "hello badword world", "H3llo w0rld, hi player!", "worse awful BADWORD hi"};
		int i = 0;
		for (const auto& s : t) Put(dir("fuzz-chatfilter") + "sentence" + std::to_string(i++) + ".txt", Bytes(s.begin(), s.end()));
	}

	Put(dir("fuzz-auth") + "handshake.pkt", Handshake());
	Put(dir("fuzz-auth") + "login_ok.pkt", Login("playerD", "hunter2"));
	Put(dir("fuzz-auth") + "login_wrong_password.pkt", Login("playerD", "letmein"));
	Put(dir("fuzz-auth") + "login_banned.pkt", Login("playerL", "hunter2"));
	Put(dir("fuzz-auth") + "login_locked.pkt", Login("playerX", "hunter2"));
	Put(dir("fuzz-auth") + "login_no_key.pkt", Login("playerA", "hunter2"));
	Put(dir("fuzz-auth") + "login_dev.pkt", Login("playere", "hunter2"));
	Put(dir("fuzz-auth") + "login_unknown.pkt", Login("", "hunter2"));

	{
		Bytes b;
		Frame(b, ChatLogin(1, "alice"));
		Frame(b, ChatLogin(2, "bob"));
		Frame(b, GeneralChat(1, eChatChannel::TEAM, u"hello team"));
		Frame(b, PrivateChat(1, "bob", u"hi bob"));
		Frame(b, IdOnly(MessageType::Chat::GET_FRIENDS_LIST, 1));
		Frame(b, IdOnly(MessageType::Chat::GET_IGNORE_LIST, 1));
		Put(dir("fuzz-chat") + "login_chat.pkt", b);
	}
	{
		Bytes b;
		Frame(b, ChatLogin(1, "alice"));
		Frame(b, ChatLogin(2, "bob"));
		Frame(b, WithName(MessageType::Chat::ADD_FRIEND_REQUEST, 1, "bob", true, 0));
		Frame(b, WithName(MessageType::Chat::ADD_FRIEND_REQUEST, 2, "alice", true, 1));
		Frame(b, WithName(MessageType::Chat::ADD_IGNORE, 1, "bob", false));
		Frame(b, WithName(MessageType::Chat::REMOVE_IGNORE, 1, "bob", false));
		Frame(b, WithName(MessageType::Chat::REMOVE_FRIEND, 1, "bob", false));
		Put(dir("fuzz-chat") + "friends.pkt", b);
	}
	{
		Bytes b;
		Frame(b, ChatLogin(1, "alice"));
		Frame(b, ChatLogin(2, "bob"));
		Frame(b, ChatLogin(3, "carol"));
		Frame(b, WithName(MessageType::Chat::TEAM_INVITE, 1, "bob", false));
		Frame(b, InviteResponse(2, 0, 1));
		Frame(b, WithName(MessageType::Chat::TEAM_INVITE, 1, "carol", false));
		Frame(b, InviteResponse(3, 0, 1));
		Frame(b, GeneralChat(1, eChatChannel::TEAM, u"team chat"));
		Frame(b, IdOnly(MessageType::Chat::TEAM_GET_STATUS, 2));
		Frame(b, LootOption(1, 1));
		Frame(b, WithName(MessageType::Chat::TEAM_SET_LEADER, 1, "bob", false));
		Frame(b, WithName(MessageType::Chat::TEAM_KICK, 2, "carol", false));
		Frame(b, IdOnly(MessageType::Chat::TEAM_LEAVE, 1));
		Put(dir("fuzz-chat") + "team.pkt", b);
	}
	{
		Bytes b;
		Frame(b, ChatLogin(7, "gm", 5));
		Frame(b, ChatLogin(8, "muted", 0, 1));
		Frame(b, GeneralChat(8, eChatChannel::TEAM, u"cannot talk"));
		Frame(b, IdOnly(MessageType::Chat::UNEXPECTED_DISCONNECT, 8));
		Put(dir("fuzz-chat") + "gm_muted.pkt", b);
	}
	return 0;
}
