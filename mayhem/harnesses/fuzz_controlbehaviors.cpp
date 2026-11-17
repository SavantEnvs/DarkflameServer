// Property-behaviour (ControlBehaviors) game message: the client-sent AMF3 argument array plus a
// command string, parsed into the dGame/dPropertyBehaviors/ControlBehaviorMessages classes.
//
// Mirrors GameMessages::HandleControlBehaviors (dGame/dGameMessages/GameMessages.cpp): Read() the
// AMF value inside catch (...) (as upstream does), return unless it is an array, read a uint32_t
// command length (rejected above MAX_MESSAGE_LENGTH) and that many command bytes. Then dispatch on
// the command as ControlBehaviors::ProcessCommand (dGame/dPropertyBehaviors/ControlBehaviors.cpp)
// does and construct the message class it builds from the arguments (ModelComponent::
// HandleControlBehaviorsMsg constructs `Msg msg{ args }`; "add"/"moveToInventory"/"remove" take the
// owner's object ID too). The model/entity work after construction needs a live zone and is not
// run. Nothing catches exceptions from the constructors: upstream does not either, so a throw
// (std::invalid_argument on a missing or mistyped key) escapes the game-message handler.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include "AMFDeserialize.h"
#include "Amf3.h"
#include "BitStream.h"
#include "ControlBehaviorMsgs.h"
#include "dCommonVars.h"
#include "fuzz_common.h"

namespace {
constexpr LWOOBJID kOwnerId = 1152921508901814272LL;  // any player object ID; only stored

template <typename Msg>
void Build(const AMFArrayValue& args) {
	Msg msg{ args };
	(void)msg.GetBehaviorId();
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	FuzzInitLogger();
	RakNet::BitStream inStream(const_cast<unsigned char*>(data), static_cast<unsigned int>(size), false);

	AMFDeserialize reader;
	std::unique_ptr<AMFArrayValue> amfArguments;
	try {
		auto deserializedData = reader.Read(inStream);
		if (!deserializedData || deserializedData->GetValueType() != eAmf::Array) return 0;
		amfArguments.reset(static_cast<AMFArrayValue*>(deserializedData.release()));
	} catch (...) {  // HandleControlBehaviors catches only around Read()
		return 0;
	}

	uint32_t commandLength{};
	inStream.Read(commandLength);
	if (commandLength > MAX_MESSAGE_LENGTH) return 0;

	std::string command;
	command.reserve(commandLength);
	for (uint32_t i = 0; i < commandLength; ++i) {
		unsigned char character;
		inStream.Read(character);
		command.push_back(character);
	}

	const AMFArrayValue& arguments = *amfArguments;
	if (command == "addStrip") Build<AddStripMessage>(arguments);
	else if (command == "removeStrip") Build<RemoveStripMessage>(arguments);
	else if (command == "mergeStrips") Build<MergeStripsMessage>(arguments);
	else if (command == "splitStrip") Build<SplitStripMessage>(arguments);
	else if (command == "updateStripUI") Build<UpdateStripUiMessage>(arguments);
	else if (command == "addAction") Build<AddActionMessage>(arguments);
	else if (command == "migrateActions") Build<MigrateActionsMessage>(arguments);
	else if (command == "rearrangeStrip") Build<RearrangeStripMessage>(arguments);
	else if (command == "removeActions") Build<RemoveActionsMessage>(arguments);
	else if (command == "updateAction") Build<UpdateActionMessage>(arguments);
	else if (command == "rename") Build<RenameMessage>(arguments);
	else if (command == "add") { AddMessage msg{ arguments, kOwnerId }; (void)msg.GetBehaviorIndex(); }
	else if (command == "moveToInventory" || command == "remove") {
		MoveToInventoryMessage msg{ arguments, kOwnerId };
		(void)msg.GetBehaviorIndex();
	}
	return 0;
}
