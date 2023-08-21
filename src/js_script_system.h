#pragma once


#include "engine/hash.h"
#include "engine/plugin.h"
#include "engine/path.h"
#include "engine/resource.h"
#include "engine/string.h"
#include "duktape/duktape.h"


namespace Lumix
{


class JSScriptModule : public IModule
{
public:

	struct Property
	{
		enum Type : int
		{
			BOOLEAN,
			NUMBER,
			STRING,
			ANY
		};

		explicit Property(IAllocator& allocator)
			: stored_value(allocator)
		{
		}

		StableHash name_hash;
		Type type;
		ResourceType resource_type;
		String stored_value;
	};


	struct IFunctionCall
	{
		virtual void add(int parameter) = 0;
		virtual void add(float parameter) = 0;
		virtual void add(void* parameter) = 0;
	};

public:
	virtual Path getScriptPath(EntityRef entity, int scr_index) = 0;	
	virtual void setScriptPath(EntityRef entity, int scr_index, const Path& path) = 0;
	virtual bool execute(EntityRef entity, i32 scr_index, StringView code) = 0;
	virtual IFunctionCall* beginFunctionCall(EntityRef entity, int scr_index, const char* function) = 0;
	virtual void endFunctionCall() = 0;
	virtual int getScriptCount(EntityRef entity) = 0;
	virtual void insertScript(EntityRef entity, int idx) = 0;
	virtual int addScript(EntityRef entity, int scr_index) = 0;
	virtual void removeScript(EntityRef entity, int scr_index) = 0;
	virtual void moveScript(EntityRef entity, int scr_index, bool up) = 0;
	virtual void setPropertyValue(EntityRef entity, int scr_index, const char* name, const char* value) = 0;
	virtual void getPropertyValue(EntityRef entity, int scr_index, const char* property_name, char* out, int max_size) = 0;
	virtual int getPropertyCount(EntityRef entity, int scr_index) = 0;
	virtual const char* getPropertyName(EntityRef entity, int scr_index, int prop_index) = 0;
	virtual Property::Type getPropertyType(EntityRef entity, int scr_index, int prop_index) = 0;
	virtual ResourceType getPropertyResourceType(EntityRef entity, int scr_index, int prop_index) = 0;
	virtual void getScriptData(EntityRef entity, OutputMemoryStream& blob) = 0;
	virtual void setScriptData(EntityRef entity, InputMemoryStream& blob) = 0;
	virtual duk_context* getGlobalContext() = 0;
};


} // namespace Lumix