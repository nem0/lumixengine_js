#include "JS_script_manager.h"

#include "core/log.h"
#include "engine/file_system.h"


namespace Lumix {

const ResourceType JSScript::TYPE("js_script");


JSScript::JSScript(const Path& path, ResourceManager& resource_manager, IAllocator& allocator)
	: Resource(path, resource_manager, allocator)
	, m_source_code(allocator) {}

JSScript::~JSScript() {}

void JSScript::unload() {
	m_source_code = "";
}

bool JSScript::load(Span<const u8> mem) {
	m_source_code = StringView((const char*)mem.begin(), (u32)mem.length());
	return true;
}

} // namespace Lumix