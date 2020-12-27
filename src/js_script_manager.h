#pragma once


#include "engine/array.h"
#include "engine/resource.h"
#include "engine/resource_manager.h"
#include "engine/string.h"


namespace Lumix {


struct JSScript final : public Resource {
	static ResourceType TYPE;

	JSScript(const Path& path, ResourceManager& resource_manager, IAllocator& allocator);
	virtual ~JSScript();

	ResourceType getType() const override { return TYPE; }
	void unload() override;
	bool load(u64 size, const u8* mem) override;
	const char* getSourceCode() const { return m_source_code.c_str(); }

private:
	String m_source_code;
};


} // namespace Lumix