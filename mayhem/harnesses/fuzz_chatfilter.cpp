// Chat filter (dChatFilter): the text a player types, checked against the approved-word list
// (allow-list mode, what the world and chat servers use) and a deny list. The filter caches the
// words it rejected, so it is built fresh for every input.
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include "dChatFilter.h"
#include "Database.h"
#include "eGameMasterLevel.h"
#include "GameDatabase/TestSQL/TestSQLDatabase.h"
#include "fuzz_common.h"
#include "fuzz_server.h"

static FuzzWordlists g_words;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	FuzzInitLogger();
	Database::_setDatabase(new TestSQLDatabase());
	g_words = FuzzWriteWordlists();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	const std::string message(reinterpret_cast<const char*>(data), size);
	dChatFilter filter(g_words.approvedBase, true);
	filter.ReadWordlistPlaintext(g_words.denied, false);
	const auto civilian = eGameMasterLevel::CIVILIAN;
	auto first = filter.IsSentenceOkay(message, civilian, true);
	auto again = filter.IsSentenceOkay(message, civilian, true);
	auto denied = filter.IsSentenceOkay(message, civilian, false);
	(void)first; (void)again; (void)denied;
	return 0;
}
