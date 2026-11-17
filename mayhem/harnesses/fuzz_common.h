// Shared setup for the dCommon fuzz harnesses: the servers' main() files define the Game::
// globals and create Game::logger; LOG() dereferences it, so each harness creates one once
// (writing to /dev/null, console off). The logger keeps no state that changes a parse result.
#pragma once
#include "Game.h"
#include "Logger.h"

static inline void FuzzInitLogger() {
	if (!Game::logger) Game::logger = new Logger("/dev/null", false, false);
}
