#include "JS_script_manager.h"

#include "engine/crc32.h"
#include "engine/log.h"
#include "engine/fs/file_system.h"


namespace Lumix
{


JSScript::JSScript(const Path& path, ResourceManagerBase& resource_manager, IAllocator& allocator)
	: Resource(path, resource_manager, allocator)
	, m_source_code(allocator)
{
}


JSScript::~JSScript()
{

}


void JSScript::unload()
{
	m_source_code = "";
}


static bool isWhitespace(char c)
{
	return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}


static const char* getToken(const char* src, char* dest, int size)
{
	const char* in = src;
	char* out = dest;
	--size;
	while (*in && isWhitespace(*in))
	{
		++in;
	}

	while (*in && !isWhitespace(*in) && size)
	{
		*out = *in;
		++out;
		++in;
		--size;
	}
	*out = '\0';
	return in;
}


bool JSScript::load(FS::IFile& file)
{
	m_source_code.set((const char*)file.getBuffer(), (int)file.size());
	m_size = file.size();
	return true;
}


JSScriptManager::JSScriptManager(IAllocator& allocator)
	: ResourceManagerBase(allocator)
	, m_allocator(allocator)
{
}


JSScriptManager::~JSScriptManager()
{
}


Resource* JSScriptManager::createResource(const Path& path)
{
	return LUMIX_NEW(m_allocator, JSScript)(path, *this, m_allocator);
}


void JSScriptManager::destroyResource(Resource& resource)
{
	LUMIX_DELETE(m_allocator, static_cast<JSScript*>(&resource));
}


} // namespace Lumix