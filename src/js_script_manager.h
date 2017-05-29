#pragma once


#include "engine/array.h"
#include "engine/resource.h"
#include "engine/resource_manager_base.h"
#include "engine/string.h"


namespace Lumix
{


class JSScript LUMIX_FINAL : public Resource
{
public:
	JSScript(const Path& path, ResourceManagerBase& resource_manager, IAllocator& allocator);
	virtual ~JSScript();

	void unload() override;
	bool load(FS::IFile& file) override;
	const char* getSourceCode() const { return m_source_code.c_str(); }

private:
	string m_source_code;
};


class JSScriptManager LUMIX_FINAL : public ResourceManagerBase
{
public:
	explicit JSScriptManager(IAllocator& allocator);
	~JSScriptManager();

protected:
	Resource* createResource(const Path& path) override;
	void destroyResource(Resource& resource) override;

private:
	IAllocator& m_allocator;
};


} // namespace Lumix