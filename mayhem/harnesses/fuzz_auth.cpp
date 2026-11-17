// Auth server packets (dNet/AuthPackets.cpp): the handshake and login request a connecting client
// sends. The dispatch is AuthServer.cpp's HandlePacket (a static function of the executable). The
// account database answers from the last character of the username, so a mutated username reaches
// the unknown-user, banned, locked, GM, play-key and password branches.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <bcrypt/BCrypt.hpp>
#include "AuthPackets.h"
#include "Database.h"
#include "GameDatabase/TestSQL/TestSQLDatabase.h"
#include "MessageType/Auth.h"
#include "MessageType/Server.h"
#include "ServiceType.h"
#include "dConfig.h"
#include "eGameMasterLevel.h"
#include "RakNetTypes.h"
#include "MessageIdentifiers.h"
#include "fuzz_common.h"
#include "fuzz_server.h"

namespace {
	std::string g_passwordHash;
	FuzzServer* g_server = nullptr;

	class AccountDatabase : public TestSQLDatabase {
	public:
		std::optional<IAccounts::Info> GetAccountInfo(const std::string_view username) override {
			if (username.empty()) return std::nullopt;
			const auto c = static_cast<unsigned char>(username.back());
			IAccounts::Info info;
			info.bcryptPassword = g_passwordHash;
			info.id = 1 + (c & 3);
			info.playKeyId = (c & 4) ? 5 : 0;
			info.banned = (c & 8) == 8 && (c & 16) == 0;
			info.locked = (c & 8) == 8 && (c & 16) == 16;
			info.maxGmLevel = (c & 32) ? eGameMasterLevel::DEVELOPER : eGameMasterLevel::CIVILIAN;
			return info;
		}
		std::optional<bool> IsPlaykeyActive(const int32_t playkeyId) override {
			return (playkeyId & 1) ? std::optional<bool>(true) : std::optional<bool>(false);
		}
		void InsertRewardCode(const uint32_t, const uint32_t) override {}
	};
}

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	FuzzInitLogger();
	Game::config = new dConfig("fuzz-auth.ini");
	g_server = new FuzzServer(ServiceType::AUTH);
	g_passwordHash = BCrypt::generateHash("hunter2", 4);
	Database::_setDatabase(new AccountDatabase());
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	std::vector<unsigned char> buf(data, data + size);
	Packet packet{};
	packet.data = buf.data();
	packet.length = static_cast<unsigned int>(size);
	packet.bitSize = static_cast<BitSize_t>(size * 8);
	packet.systemAddress.port = 2001;
	if (packet.length < 4) return 0;
	if (packet.data[0] != ID_USER_PACKET_ENUM) return 0;
	if (static_cast<ServiceType>(packet.data[1]) == ServiceType::COMMON) {
		if (static_cast<MessageType::Server>(packet.data[3]) == MessageType::Server::VERSION_CONFIRM) {
			AuthPackets::HandleHandshake(g_server, &packet);
		}
	} else if (static_cast<ServiceType>(packet.data[1]) == ServiceType::AUTH) {
		if (static_cast<MessageType::Auth>(packet.data[3]) == MessageType::Auth::LOGIN_REQUEST) {
			AuthPackets::HandleLoginRequest(g_server, &packet);
		}
	}
	return 0;
}
