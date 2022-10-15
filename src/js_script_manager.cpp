#include "JS_script_manager.h"

#include "engine/file_system.h"
#include "engine/log.h"


namespace Lumix {

const ResourceType JSScript::TYPE("js_script");


JSScript::JSScript(const Path& path, ResourceManager& resource_manager, IAllocator& allocator)
	: Resource(path, resource_manager, allocator)
	, m_source_code(allocator) {}

JSScript::~JSScript() {}

void JSScript::unload() {
	m_source_code = "";
}

bool JSScript::load(u64 size, const u8* mem) {
	m_source_code = Span((const char*)mem, (u32)size);
	return true;
}

} // namespace Lumix