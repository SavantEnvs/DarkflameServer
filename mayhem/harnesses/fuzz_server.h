// A dServer with no sockets, for harnesses that drive the servers' packet handlers directly. Every
// replica of the handlers' reply goes to Send(), which discards it; the handlers have already built it.
#pragma once
#include "dServer.h"
#include "ServiceType.h"

class FuzzServer : public dServer {
public:
	explicit FuzzServer(ServiceType type) {
		mPort = 0;
		mMaxConnections = 0;
		mZoneID = 0;
		mInstanceID = 0;
		mUseEncryption = false;
		mIsInternal = false;
		mIsOkay = true;
		mMasterConnectionActive = false;
		mMasterPort = 0;
		mServerType = type;
	}
	void Send(RakNet::BitStream&, const SystemAddress&, bool) override {}
};

// Wordlists for the chat filter: written once to a scratch directory at start-up, read-only afterwards.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

struct FuzzWordlists {
	std::string approvedBase; // dChatFilter reads <base>.txt
	std::string denied;
};

static inline FuzzWordlists FuzzWriteWordlists() {
	const char* tmp = std::getenv("TMPDIR");
	std::string dirTemplate = std::string(tmp && *tmp ? tmp : "/tmp") + "/dlu-fuzz-words-XXXXXX";
	char* dir = mkdtemp(dirTemplate.data());
	FuzzWordlists w;
	const std::string base = dir ? dir : "/tmp";
	w.approvedBase = base + "/approved";
	w.denied = base + "/denied.txt";
	std::ofstream(w.approvedBase + ".txt") << "hello\nworld\nthe\nquick\nbrown\nfox\njumps\nover\nlazy\ndog\nhi\nplayer\n";
	std::ofstream(w.denied) << "badword\nworse\nawful\n";
	return w;
}
