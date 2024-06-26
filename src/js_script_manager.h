#pragma once


#include "core/array.h"
#include "core/string.h"
#include "engine/resource.h"
#include "engine/resource_manager.h"


namespace Lumix {


struct JSScript final : public Resource {
	static const ResourceType TYPE;

	JSScript(const Path& path, ResourceManager& resource_manager, IAllocator& allocator);
	virtual ~JSScript();

	ResourceType getType() const override { return TYPE; }
	void unload() override;
	bool load(Span<const u8> mem) override;
	const char* getSourceCode() const { return m_source_code.c_str(); }

private:
	String m_source_code;
};


} // namespace Lumix