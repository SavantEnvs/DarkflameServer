// Definitions of the Game:: globals that each server executable defines in its main file
// (dCommon/Game.h declares them extern). The harnesses set the ones their code uses.
#include "Game.h"
#include "Logger.h"
#include "dConfig.h"
#include "PlayerContainer.h"
#include <random>
#include <string>

namespace Game {
	Logger* logger = nullptr;
	dServer* server = nullptr;
	dChatFilter* chatFilter = nullptr;
	PlayerContainer playerContainer;
	dConfig* config = nullptr;
	std::mt19937 randomEngine;
	signal_t lastSignal = 0;
	std::string projectVersion = "fuzz";
}
