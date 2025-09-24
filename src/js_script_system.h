#pragma once


#include "core/hash.h"
#include "core/path.h"
#include "core/stream.h"
#include "core/string.h"
#include "engine/plugin.h"
#include "engine/resource.h"
#include "duktape/duktape.h"


namespace Lumix
{

struct JSScriptSystem : ISystem {
	virtual duk_context* getGlobalContext() = 0;
};


//@ module JSScriptModule js_script "JS Script"
struct JSScriptModule : public IModule {
	struct Property {
		enum Type : i32 {
			BOOLEAN,
			NUMBER,
			STRING,
			ENTITY
		};

		explicit Property(IAllocator& allocator)
			: stored_value(allocator)
		{}

		StableHash name_hash;
		Type type;
		ResourceType resource_type;
		OutputMemoryStream stored_value;
	};


	struct IFunctionCall {
		virtual void add(int parameter) = 0;
		virtual void add(float parameter) = 0;
		virtual void add(void* parameter) = 0;
	};
	
	virtual void createScript(EntityRef e) = 0;
	virtual void destroyScript(EntityRef e) = 0;

	//@ component Script id js_script
	//@ array Script scripts
	virtual Path getScriptPath(EntityRef entity, int scr_index) = 0;	//@ resource_type JSScript::TYPE
	virtual void setScriptPath(EntityRef entity, int scr_index, const Path& path) = 0;
	virtual void getScriptData(EntityRef entity, OutputMemoryStream& blob) = 0;
	virtual void setScriptData(EntityRef entity, InputMemoryStream& blob) = 0;
	//@ end
	//@ end
	virtual bool execute(EntityRef entity, i32 scr_index, StringView code) = 0;
	virtual IFunctionCall* beginFunctionCall(EntityRef entity, int scr_index, const char* function) = 0;
	virtual void endFunctionCall() = 0;
	virtual int getScriptCount(EntityRef entity) = 0;
	virtual void insertScript(EntityRef entity, int idx) = 0;
	virtual int addScript(EntityRef entity, int scr_index) = 0;
	virtual uintptr getScriptID(EntityRef entity, i32 scr_index) = 0;
	virtual void removeScript(EntityRef entity, int scr_index) = 0;
	virtual void moveScript(EntityRef entity, int scr_index, bool up) = 0;
	virtual int getPropertyCount(EntityRef entity, int scr_index) = 0;
	virtual const char* getPropertyName(EntityRef entity, int scr_index, int prop_index) = 0;
	virtual Property::Type getPropertyType(EntityRef entity, int scr_index, int prop_index) = 0;
	virtual ResourceType getPropertyResourceType(EntityRef entity, int scr_index, int prop_index) = 0;
	virtual duk_context* getGlobalContext() = 0;
};


} // namespace Lumix