#pragma once 

#include "engine/lumix.h"

namespace duk_debugger {

bool init();
void finish();
void disconnect();
bool tryConnect();
bool isConnected();

Lumix::u64 readCallback(void* udata, char* buffer, Lumix::u64 length);
Lumix::u64 writeCallback(void* udata, const char* buffer, Lumix::u64 length);
Lumix::u64 peekCallback(void* udata);

}