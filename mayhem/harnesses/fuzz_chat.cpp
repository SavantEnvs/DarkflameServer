// Chat server packets (dChatServer: ChatPacketHandler, ChatIgnoreList, TeamContainer, PlayerContainer).
// The world servers relay what players type to the chat server. The input is a sequence of packets,
// each a 2-byte little-endian length and that many bytes, so one input can first log players in
// (LOGIN_SESSION_NOTIFY) and then message, friend and team them. The dispatch below is ChatServer.cpp's
// HandlePacket (a static function of the executable), case for case. Server state is reset per input.
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "dCommonVars.h"
#include "dChatFilter.h"
#include "ChatIgnoreList.h"
#include "ChatPacketHandler.h"
#include "Database.h"
#include "GameDatabase/TestSQL/TestSQLDatabase.h"
#include "MessageType/Chat.h"
#include "PlayerContainer.h"
#include "ServiceType.h"
#include "TeamContainer.h"
#include "BitStream.h"
#include "MessageIdentifiers.h"
#include "RakNetTypes.h"
#include "dConfig.h"
#include "fuzz_common.h"
#include "fuzz_server.h"

static FuzzWordlists g_words;

static void HandlePacket(Packet* packet) {
	if (packet->length < 1) return;
	if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION || packet->data[0] == ID_CONNECTION_LOST) {
	} else if (packet->data[0] == ID_NEW_INCOMING_CONNECTION) {
	} else if (packet->length < 4 || packet->data[0] != ID_USER_PACKET_ENUM) return;

	CINSTREAM;
	inStream.SetReadOffset(BYTES_TO_BITS(1));

	ServiceType connection;
	inStream.Read(connection);
	if (connection != ServiceType::CHAT) return;

	MessageType::Chat chatMessageID;
	inStream.Read(chatMessageID);

	if (inStream.GetNumberOfUnreadBits() < 8) return;
	inStream.IgnoreBytes(1);

	switch (chatMessageID) {
	case MessageType::Chat::GM_MUTE: Game::playerContainer.MuteUpdate(packet); break;
	case MessageType::Chat::CREATE_TEAM: TeamContainer::CreateTeamServer(packet); break;
	case MessageType::Chat::GET_FRIENDS_LIST: ChatPacketHandler::HandleFriendlistRequest(packet); break;
	case MessageType::Chat::GET_IGNORE_LIST: ChatIgnoreList::GetIgnoreList(packet); break;
	case MessageType::Chat::ADD_IGNORE: ChatIgnoreList::AddIgnore(packet); break;
	case MessageType::Chat::REMOVE_IGNORE: ChatIgnoreList::RemoveIgnore(packet); break;
	case MessageType::Chat::TEAM_GET_STATUS: TeamContainer::HandleTeamStatusRequest(packet); break;
	case MessageType::Chat::ADD_FRIEND_REQUEST: ChatPacketHandler::HandleFriendRequest(packet); break;
	case MessageType::Chat::ADD_FRIEND_RESPONSE: ChatPacketHandler::HandleFriendResponse(packet); break;
	case MessageType::Chat::REMOVE_FRIEND: ChatPacketHandler::HandleRemoveFriend(packet); break;
	case MessageType::Chat::GENERAL_CHAT_MESSAGE: ChatPacketHandler::HandleChatMessage(packet); break;
	case MessageType::Chat::PRIVATE_CHAT_MESSAGE: ChatPacketHandler::HandlePrivateChatMessage(packet); break;
	case MessageType::Chat::TEAM_INVITE: TeamContainer::HandleTeamInvite(packet); break;
	case MessageType::Chat::TEAM_INVITE_RESPONSE: TeamContainer::HandleTeamInviteResponse(packet); break;
	case MessageType::Chat::TEAM_LEAVE: TeamContainer::HandleTeamLeave(packet); break;
	case MessageType::Chat::TEAM_SET_LEADER: TeamContainer::HandleTeamPromote(packet); break;
	case MessageType::Chat::TEAM_KICK: TeamContainer::HandleTeamKick(packet); break;
	case MessageType::Chat::TEAM_SET_LOOT: TeamContainer::HandleTeamLootOption(packet); break;
	case MessageType::Chat::GMLEVEL_UPDATE: ChatPacketHandler::HandleGMLevelUpdate(packet); break;
	case MessageType::Chat::LOGIN_SESSION_NOTIFY: Game::playerContainer.InsertPlayer(packet); break;
	case MessageType::Chat::GM_ANNOUNCE:
		inStream.ResetReadPointer();
		Game::server->Send(inStream, packet->systemAddress, true);
		break;
	case MessageType::Chat::UNEXPECTED_DISCONNECT: Game::playerContainer.ScheduleRemovePlayer(packet); break;
	case MessageType::Chat::WHO: ChatPacketHandler::HandleWho(packet); break;
	case MessageType::Chat::SHOW_ALL: ChatPacketHandler::HandleShowAll(packet); break;
	case MessageType::Chat::ACHIEVEMENT_NOTIFY: ChatPacketHandler::OnAchievementNotify(inStream, packet->systemAddress); break;
	default: break;
	}
}

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	FuzzInitLogger();
	Game::config = new dConfig("fuzz-chat.ini");
	Game::server = new FuzzServer(ServiceType::CHAT);
	Database::_setDatabase(new TestSQLDatabase());
	g_words = FuzzWriteWordlists();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	// Fresh server state for every input.
	Game::playerContainer = PlayerContainer{};
	Game::playerContainer.Initialize();
	for (auto* team : TeamContainer::GetTeamsMut()) delete team;
	TeamContainer::GetTeamsMut().clear();
	delete Game::chatFilter;
	Game::chatFilter = new dChatFilter(g_words.approvedBase, true);

	size_t pos = 0;
	while (size - pos >= 2) {
		size_t len = data[pos] | (static_cast<size_t>(data[pos + 1]) << 8);
		pos += 2;
		if (len > size - pos) len = size - pos;
		std::vector<unsigned char> buf(data + pos, data + pos + len);
		pos += len;
		Packet packet{};
		packet.data = buf.data();
		packet.length = static_cast<unsigned int>(len);
		packet.bitSize = static_cast<BitSize_t>(len * 8);
		HandlePacket(&packet);
	}
	return 0;
}
