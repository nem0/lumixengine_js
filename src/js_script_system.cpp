#include "JS_script_system.h"
#include "core/array.h"
#include "core/associative_array.h"
#include "core/hash.h"
#include "core/log.h"
#include "core/path.h"
#include "core/profiler.h"
#include "core/stream.h"
#include "core/string.h"
#include "engine/engine.h"
#include "engine/file_system.h"
#include "engine/input_system.h"
#include "engine/plugin.h"
#include "engine/reflection.h"
#include "engine/resource_manager.h"
#include "engine/world.h"
#include "imgui/imgui.h"
#include "js_script_manager.h"
#include "js_wrapper.h"


namespace Lumix {

// for serialization
static_assert(sizeof(bool) == sizeof(u8));

inline void toCString(EntityPtr value, Span<char> output) {
	toCString(value.index, output);
}

inline const char* fromCString(StringView input, EntityPtr& value) {
	return fromCString(input, value.index);
}

static const ComponentType JS_SCRIPT_TYPE = reflection::getComponentType("js_script");

namespace JSImGui {
int Text(duk_context* ctx) {
	auto* text = JSWrapper::toType<const char*>(ctx, 0);
	ImGui::TextUnformatted(text);
	return 0;
}


int OpenPopup(duk_context* ctx) {
	auto* name = JSWrapper::toType<const char*>(ctx, 0);
	ImGui::OpenPopup(name);
	return 0;
}


int Button(duk_context* ctx) {
	auto* label = JSWrapper::toType<const char*>(ctx, 0);
	bool ret = ImGui::Button(label);
	JSWrapper::push(ctx, ret);
	return 1;
}


int Begin(duk_context* ctx) {
	auto* name = JSWrapper::toType<const char*>(ctx, 0);
	bool ret = ImGui::Begin(name);
	JSWrapper::push(ctx, ret);
	return 1;
}


int Checkbox(duk_context* ctx) {
	auto* name = JSWrapper::toType<const char*>(ctx, 0);
	bool value = JSWrapper::toType<bool>(ctx, 1);
	ImGui::Checkbox(name, &value);
	JSWrapper::push(ctx, value);
	return 1;
}


int CollapsingHeader(duk_context* ctx) {
	auto* name = JSWrapper::toType<const char*>(ctx, 0);
	bool ret = ImGui::CollapsingHeader(name);
	JSWrapper::push(ctx, ret);
	return 1;
}


int Selectable(duk_context* ctx) {
	auto* name = JSWrapper::toType<const char*>(ctx, 0);
	bool selected = JSWrapper::toType<bool>(ctx, 1);
	ImGui::Selectable(name, &selected);
	JSWrapper::push(ctx, selected);
	return 1;
}


int BeginChildFrame(duk_context* ctx) {
	auto* name = JSWrapper::toType<const char*>(ctx, 0);
	ImVec2 size(0, 0);
	if (duk_get_top(ctx) > 1) {
		size.x = JSWrapper::toType<float>(ctx, 1);
		size.y = JSWrapper::toType<float>(ctx, 2);
	}
	bool ret = ImGui::BeginChildFrame(ImGui::GetID(name), size);
	JSWrapper::push(ctx, ret);
	return 1;
}


int SliderFloat(duk_context* ctx) {
	auto* label = JSWrapper::toType<const char*>(ctx, 0);
	float value = JSWrapper::toType<float>(ctx, 1);
	float v_min = JSWrapper::toType<float>(ctx, 2);
	float v_max = JSWrapper::toType<float>(ctx, 3);
	ImGui::SliderFloat(label, &value, v_min, v_max);
	JSWrapper::push(ctx, value);
	return 1;
}


int DragFloat(duk_context* ctx) {
	auto* label = JSWrapper::toType<const char*>(ctx, 0);
	float value = JSWrapper::toType<float>(ctx, 1);
	ImGui::DragFloat(label, &value);
	JSWrapper::push(ctx, value);
	return 1;
}


int SameLine(duk_context* ctx) {
	ImGui::SameLine();
	return 0;
}


int LabelText(duk_context* ctx) {
	auto* label = JSWrapper::toType<const char*>(ctx, 0);
	auto* text = JSWrapper::toType<const char*>(ctx, 1);
	ImGui::LabelText(label, "%s", text);
	return 0;
}
} // namespace JSImGui


static int ptrJSConstructor(duk_context* ctx) {
	if (!duk_is_constructor_call(ctx)) return DUK_RET_TYPE_ERROR;

	duk_push_this(ctx);
	duk_dup(ctx, 0);
	duk_put_prop_string(ctx, -2, "c_ptr");

	return 0;
}


static int entityProxySetter(duk_context* ctx) {
	duk_get_prop_string(ctx, 0, "c_world");
	World* world= (World*)duk_get_pointer(ctx, -1);

	duk_get_prop_string(ctx, 0, "c_entity");
	EntityRef entity = {duk_get_int(ctx, -1)};

	duk_pop_2(ctx);

	const char* prop_name = duk_get_string(ctx, 1);
	if (equalStrings(prop_name, "rotation")) {
		Quat r = JSWrapper::toType<Quat>(ctx, 2);
		world->setRotation(entity, r);
	}
	else if (equalStrings(prop_name, "position")) {
		DVec3 v = JSWrapper::toType<DVec3>(ctx, 2);
		world->setPosition(entity, v);
	}
	else if (equalStrings(prop_name, "scale")) {
		Vec3 v = JSWrapper::toType<Vec3>(ctx, 2);
		world->setScale(entity, v);
	}
	else {
		duk_push_sprintf(ctx, " trying to set unknown property %s", prop_name);
		duk_throw(ctx);
	}
	return 0;
}

static int entityProxyGetter(duk_context* ctx) {
	const char* prop_name = duk_get_string(ctx, 1);

	duk_get_prop_string(ctx, 0, "c_entity");
	ASSERT(duk_is_number(ctx, -1));
	EntityRef entity = {duk_get_int(ctx, -1)};
	duk_pop(ctx);
	if (equalStrings(prop_name, "c_entity")) {
		JSWrapper::push(ctx, entity.index);
		return 1;
	}

	duk_get_prop_string(ctx, 0, "c_world");
	World* world= (World*)duk_get_pointer(ctx, -1);
	if (!world) return 0;

	duk_pop(ctx);

	if (equalStrings(prop_name, "rotation")) {
		JSWrapper::push(ctx, world->getRotation(entity));
		return 1;
	}
	if (equalStrings(prop_name, "position")) {
		JSWrapper::push(ctx, world->getPosition(entity));
		return 1;
	}
	if (equalStrings(prop_name, "scale")) {
		JSWrapper::push(ctx, world->getScale(entity));
		return 1;
	}
	if (!reflection::componentTypeExists(prop_name)) return 0;

	ComponentType cmp_type = reflection::getComponentType(prop_name);
	if (!world->hasComponent(entity, cmp_type)) return 0;
	
	IModule* module = world->getModule(cmp_type);
	if (!module) return 0;

	JSWrapper::DebugGuard guard(ctx, 1);
	duk_get_global_string(ctx, "LumixAPI");
	duk_get_prop_string(ctx, -1, prop_name);
	JSWrapper::push(ctx, module);
	JSWrapper::push(ctx, entity.index);
	duk_new(ctx, 2);
	duk_remove(ctx, -2);

	return 1;
}


static int entityJSConstructor(duk_context* ctx) {
	if (!duk_is_constructor_call(ctx)) return DUK_RET_TYPE_ERROR;

	duk_get_global_string(ctx, "Proxy");

	duk_push_this(ctx);

	duk_dup(ctx, 0);
	duk_put_prop_string(ctx, -2, "c_world");

	duk_dup(ctx, 1);
	duk_put_prop_string(ctx, -2, "c_entity");

	duk_push_object(ctx); // proxy handler
	duk_push_c_function(ctx, entityProxyGetter, 3);
	duk_put_prop_string(ctx, -2, "get");

	duk_push_c_function(ctx, entityProxySetter, 3);
	duk_put_prop_string(ctx, -2, "set");

	duk_new(ctx, 2);

	return 1;
}


static int componentJSConstructor(duk_context* ctx) {
	if (!duk_is_constructor_call(ctx)) return DUK_RET_TYPE_ERROR;
	if (!duk_is_pointer(ctx, 0)) return DUK_RET_TYPE_ERROR;

	duk_push_this(ctx);
	duk_dup(ctx, 0);
	duk_put_prop_string(ctx, -2, "c_module");

	duk_dup(ctx, 1);
	duk_put_prop_string(ctx, -2, "c_entity");

	return 0;
}


static void registerJSObject(duk_context* ctx, const char* prototype, const char* name, duk_c_function constructor) {
	duk_push_c_function(ctx, constructor, DUK_VARARGS);

	if (prototype == nullptr || prototype[0] == '\0') {
		duk_push_object(ctx); // prototype
		duk_put_prop_string(ctx, -2, "prototype");
	} else {
		duk_get_global_string(ctx, prototype);
		duk_set_prototype(ctx, -2);
	}

	duk_put_global_string(ctx, name);
}


static void registerJSComponent(duk_context* ctx, ComponentType cmp_type, const char* name, duk_c_function constructor) {
	duk_push_c_function(ctx, constructor, DUK_VARARGS);

	duk_push_object(ctx); // prototype
	duk_put_prop_string(ctx, -2, "prototype");

	duk_push_int(ctx, cmp_type.index);
	duk_put_prop_string(ctx, -2, "c_cmptype");

	duk_put_global_string(ctx, name);
}


static void registerMethod(duk_context* ctx, const char* obj, const char* method_name, duk_c_function method) {
	if (duk_get_global_string(ctx, obj) == 0) {
		ASSERT(false);
		return;
	}

	if (duk_get_prop_string(ctx, -1, "prototype") != 1) {
		ASSERT(false);
		return;
	}
	duk_push_string(ctx, method_name);
	duk_push_c_function(ctx, method, DUK_VARARGS);
	duk_put_prop(ctx, -3);
	duk_pop_2(ctx);
}


static void registerGlobalVariable(duk_context* ctx, const char* type_name, const char* var_name, void* ptr) {
	if (duk_get_global_string(ctx, type_name) != 1) {
		ASSERT(false);
		return;
	}
	duk_push_pointer(ctx, ptr);
	duk_new(ctx, 1);
	duk_put_global_string(ctx, var_name);
}

struct JSScriptManager final : public ResourceManager {
	JSScriptManager(IAllocator& allocator)
		: ResourceManager(allocator)
		, m_allocator(allocator) {}

	~JSScriptManager() {}

	Resource* createResource(const Path& path) { return LUMIX_NEW(m_allocator, JSScript)(path, *this, m_allocator); }

	void destroyResource(Resource& resource) { LUMIX_DELETE(m_allocator, static_cast<JSScript*>(&resource)); }

	IAllocator& m_allocator;
};

struct JSScriptSystemImpl final : JSScriptSystem {
	explicit JSScriptSystemImpl(Engine& engine);
	virtual ~JSScriptSystemImpl();
	void initBegin() override;
	duk_context* getGlobalContext() override { return m_global_context; }

	void serialize(OutputMemoryStream& serializer) const override {}
	bool deserialize(i32 version, InputMemoryStream& serializer) override { return version == 0; }
	void createModules(World& world) override;
	const char* getName() const override { return "js_script"; }
	JSScriptManager& getScriptManager() { return m_script_manager; }
	void registerGlobalAPI();
	void registerImGuiAPI();

	Engine& m_engine;
	IAllocator& m_allocator;
	JSScriptManager m_script_manager;
	duk_context* m_global_context;

	static inline JSScriptSystemImpl* s_instance = nullptr;
};


struct JSScriptModuleImpl final : public JSScriptModule{
	struct ContextRef {
		duk_context* context;
		uintptr id;
	};

	struct ScriptInstance {
		explicit ScriptInstance(IAllocator& allocator)
			: m_properties(allocator)
			, m_script(nullptr) {}

		JSScript* m_script;
		Array<Property> m_properties;
		uintptr m_id;
	};


	struct ScriptComponent {
		ScriptComponent(JSScriptModuleImpl& module, EntityRef entity, IAllocator& allocator)
			: m_scripts(allocator)
			, m_module(module)
			, m_entity(entity) {}


		static int getProperty(ScriptInstance& inst, StableHash hash) {
			for (int i = 0, c = inst.m_properties.size(); i < c; ++i) {
				if (inst.m_properties[i].name_hash == hash) return i;
			}
			return -1;
		}


		void onScriptLoaded(Resource::State old_state, Resource::State new_state, Resource& resource) {
			duk_context* ctx = m_module.m_system.m_global_context;
			for (auto& script : m_scripts) {
				if (!script.m_script) continue;
				if (!script.m_script->isReady()) continue;
				if (script.m_script != &resource) continue;

				if (new_state == Resource::State::READY) {
					m_module.onScriptLoaded(m_entity, script, false);
				}
			}
		}


		Array<ScriptInstance> m_scripts;
		JSScriptModuleImpl& m_module;
		EntityRef m_entity;
	};


	struct FunctionCall : IFunctionCall {
		void add(int parameter) override {
			ASSERT(false);
			JSWrapper::push(context, parameter);
			++parameter_count;
		}


		void add(float parameter) override {
			JSWrapper::push(context, parameter);
			++parameter_count;
		}


		void add(void* parameter) override {
			JSWrapper::push(context, parameter);
			++parameter_count;
		}


		int parameter_count;
		duk_context* context;
		bool is_in_progress;
		ScriptComponent* cmp;
		int scr_index;
	};


public:
	~JSScriptModuleImpl() {
		Path invalid_path;
		for (auto* script_cmp : m_scripts) {
			if (!script_cmp) continue;

			for (auto& script : script_cmp->m_scripts) {
				setScriptPathInternal(*script_cmp, script, invalid_path);
			}
			LUMIX_DELETE(m_system.m_allocator, script_cmp);
		}
	}

	JSScriptModuleImpl(JSScriptSystemImpl& system, World& ctx)
		: m_system(system)
		, m_world(ctx)
		, m_scripts(system.m_allocator)
		, m_updates(system.m_allocator)
		, m_input_handlers(system.m_allocator)
		, m_property_names(system.m_allocator)
		, m_is_game_running(false)
		, m_is_api_registered(false) {
		m_function_call.is_in_progress = false;

		registerAPI();
	}


	const char* getName() const override { return "js_script"; }
	int getVersion() const override { return -1; }

	JSExecuteResult execute(EntityRef entity, i32 scr_index, StringView code) override {
		auto iter = m_scripts.find(entity);
		if (!iter.isValid()) return JSExecuteResult::NO_SCRIPT;
		
		ScriptComponent* script_cmp = iter.value();
		if (scr_index >= script_cmp->m_scripts.size()) return JSExecuteResult::NO_SCRIPT;

		ScriptInstance& script = script_cmp->m_scripts[scr_index];

		duk_context* ctx = m_system.m_global_context;

		if (duk_pcompile_lstring(ctx, DUK_COMPILE_EVAL, code.begin, code.size()) != 0) {
			logError("Compile failed: ", duk_safe_to_stacktrace(ctx, -1));
			return JSExecuteResult::FAILED_TO_COMPILE;
		}

		duk_push_global_stash(ctx); // [fn, stash]
		duk_push_pointer(ctx, (void*)script.m_id);  // [fn, stash, id]
		duk_get_prop(ctx, -2); // [fn, stash, this]
		duk_remove(ctx, -2); // [fn, this]

		if (duk_pcall_method(ctx, 0) != DUK_EXEC_SUCCESS) { 
			logError(duk_safe_to_stacktrace(ctx, -1));
			return JSExecuteResult::RUNTIME_ERROR;
		}
		duk_pop(ctx);
		return JSExecuteResult::SUCCESS;
	}

	IFunctionCall* beginFunctionCall(EntityRef entity, int scr_index, const char* function) override {
		ASSERT(!m_function_call.is_in_progress);

		auto* script_cmp = m_scripts[{entity.index}];
		auto& script = script_cmp->m_scripts[scr_index];

		duk_context* ctx = m_system.m_global_context;

		duk_push_global_stash(ctx);
		duk_push_pointer(ctx, (void*)script.m_id);
		duk_get_prop(ctx, -2);
		if (duk_is_undefined(ctx, -1)) {
			duk_pop_2(ctx);
			return nullptr;
		}
		duk_get_prop_string(ctx, -1, function);
		if (!duk_is_callable(ctx, -1)) {
			duk_pop_3(ctx);
			return nullptr;
		}
		duk_dup(ctx, -2); // [this, func] -> [this, func, this]

		m_function_call.context = ctx;
		m_function_call.cmp = script_cmp;
		m_function_call.is_in_progress = true;
		m_function_call.parameter_count = 0;
		m_function_call.scr_index = scr_index;

		return &m_function_call;
	}


	void endFunctionCall() override {
		ASSERT(m_function_call.is_in_progress);

		m_function_call.is_in_progress = false;

		auto& script = m_function_call.cmp->m_scripts[m_function_call.scr_index];

		if (duk_pcall_method(m_function_call.context, m_function_call.parameter_count) == DUK_EXEC_ERROR) {
			const char* error = duk_safe_to_string(m_function_call.context, -1);
			logError(error);
		}
		duk_pop_2(m_function_call.context);
	}


	int getPropertyCount(EntityRef entity, int scr_index) override { return m_scripts[entity]->m_scripts[scr_index].m_properties.size(); }


	const char* getPropertyName(EntityRef entity, int scr_index, int prop_index) override { return getPropertyName(m_scripts[entity]->m_scripts[scr_index].m_properties[prop_index].name_hash); }


	ResourceType getPropertyResourceType(EntityRef entity, int scr_index, int prop_index) override { return m_scripts[entity]->m_scripts[scr_index].m_properties[prop_index].resource_type; }


	Property::Type getPropertyType(EntityRef entity, int scr_index, int prop_index) override { return m_scripts[entity]->m_scripts[scr_index].m_properties[prop_index].type; }


	void getScriptData(EntityRef entity, OutputMemoryStream& blob) override {
		/*auto* scr = m_scripts[entity];
		blob.write(scr->m_scripts.size());
		for (int i = 0; i < scr->m_scripts.size(); ++i) {
			auto& inst = scr->m_scripts[i];
			blob.writeString(inst.m_script ? inst.m_script->getPath().c_str() : "");
			blob.write(inst.m_properties.size());
			for (auto& prop : inst.m_properties) {
				blob.write(prop.name_hash);
				blob.write(prop.type);
				char tmp[1024];
				tmp[0] = '\0';
				const char* prop_name = getPropertyName(prop.name_hash);
				if (prop_name) getPropertyValue(entity, i, getPropertyName(prop.name_hash), tmp, lengthOf(tmp));
				blob.writeString(prop_name ? tmp : prop.stored_value.c_str());
			}
		}*/
		ASSERT(false);
	}


	duk_context* getGlobalContext() override { return m_system.m_global_context; }

	void setScriptData(EntityRef entity, InputMemoryStream& blob) override {
		ASSERT(false); // TODO
					   /*
					   auto* scr = m_scripts[entity];
					   int count;
					   blob.read(count);
					   for (int i = 0; i < count; ++i)
					   {
						   int idx = addScript(cmp);
						   auto& inst = scr->m_scripts[idx];
						   char tmp[MAX_PATH_LENGTH];
						   blob.readString(tmp, lengthOf(tmp));
						   setScriptPath(cmp, idx, Path(tmp));
			   
						   int prop_count;
						   blob.read(prop_count);
						   for (int j = 0; j < prop_count; ++j)
						   {
							   u32 hash;
							   blob.read(hash);
							   int prop_index = scr->getProperty(inst, hash);
							   if (prop_index < 0)
							   {
								   scr->m_scripts[idx].m_properties.emplace(m_system.m_allocator);
								   prop_index = scr->m_scripts[idx].m_properties.size() - 1;
							   }
							   auto& prop = scr->m_scripts[idx].m_properties[prop_index];
							   prop.name_hash = hash;
							   blob.read(prop.type);
							   char tmp[1024];
							   blob.readString(tmp, lengthOf(tmp));
							   prop.stored_value = tmp;
							   if (scr->m_scripts[idx].m_state) applyProperty(scr->m_scripts[idx], prop, tmp);
						   }
					   }
					   */
	}



	World& getWorld() override { return m_world; }


	void getInstanceName(IModule& module, char* out_str, int size) {
		copyString(Span(out_str, size), "g_module_");
		catString(Span(out_str, size), module.getSystem().getName());
	}


	void registerAPI() {
		if (m_is_api_registered) return;
		m_is_api_registered = true;

		duk_context* ctx = m_system.m_global_context;
		registerGlobalVariable(ctx, "World", "g_world", &m_world);

		Array<UniquePtr<IModule>>& modules = m_world.getModules();
		for (UniquePtr<IModule>& module : modules) {
			StaticString<50> type_name(module->getSystem().getName(), "_module");
			char inst_name[50];
			getInstanceName(*module, inst_name, lengthOf(inst_name));
			registerJSObject(ctx, "ModuleBase", type_name, &ptrJSConstructor);
			registerGlobalVariable(ctx, type_name, inst_name, module.get());
		}
	}


	const char* getPropertyName(StableHash name_hash) const {
		int idx = m_property_names.find(name_hash);
		if (idx >= 0) return m_property_names.at(idx).c_str();
		return nullptr;
	}

	void applyProperty(duk_context* ctx, ScriptInstance& script, Property& prop, InputMemoryStream value) {
		const char* name = getPropertyName(prop.name_hash);
		if (!name) return;

		duk_push_global_stash(ctx);
		duk_push_pointer(ctx, (void*)script.m_id);
		duk_get_prop(ctx, -2);

		if (prop.type == Property::ENTITY) {
			duk_get_global_string(ctx, "Entity");
			duk_push_pointer(ctx, &m_world);
			EntityPtr e = value.read<EntityPtr>();
			JSWrapper::push(ctx, e.index);
			duk_new(ctx, 2);			
		}
		else if (prop.type == Property::BOOLEAN) {
			bool b = value.read<bool>() != 0;
			duk_push_boolean(ctx, b);
		}
		else if (prop.type == Property::NUMBER) {
			duk_push_number(ctx, value.read<double>());
		}
		else if (prop.type == Property::STRING) {
			duk_push_string(ctx, value.readString());
		}
		else {
			ASSERT(false);
		}

		duk_put_prop_string(ctx, -2, name);
		duk_pop_2(ctx);
	}

	const char* getPropertyName(EntityRef entity, int scr_index, int index) const {
		auto& script = m_scripts[entity]->m_scripts[scr_index];

		return getPropertyName(script.m_properties[index].name_hash);
	}


	int getPropertyCount(EntityRef entity, int scr_index) const {
		auto& script = m_scripts[entity]->m_scripts[scr_index];

		return script.m_properties.size();
	}

	static int getScriptIndex(ScriptComponent& scr, ScriptInstance& inst) { return int(&inst - &scr.m_scripts[0]); }


	void clearInstance(ScriptComponent& scr, ScriptInstance& inst) {
		int scr_idx = getScriptIndex(scr, inst);
		auto* call = beginFunctionCall(scr.m_entity, scr_idx, "onDestroy");
		if (call) endFunctionCall();

		for (int i = 0; i < m_updates.size(); ++i) {
			if (m_updates[i].id == inst.m_id) {
				m_updates.swapAndPop(i);
				break;
			}
		}

		for (int i = 0; i < m_input_handlers.size(); ++i) {
			if (m_input_handlers[i].id == inst.m_id) {
				m_input_handlers.swapAndPop(i);
				break;
			}
		}

		duk_context* ctx = m_system.m_global_context;
		duk_push_global_stash(ctx);
		duk_push_pointer(ctx, (void*)inst.m_id);
		duk_del_prop(ctx, -2);
		duk_pop(ctx);

		inst.m_properties.clear();
	}


	void setScriptPathInternal(ScriptComponent& cmp, ScriptInstance& inst, const Path& path) {
		if (inst.m_script) {
			clearInstance(cmp, inst);
			auto& cb = inst.m_script->getObserverCb();
			cb.unbind<&ScriptComponent::onScriptLoaded>(&cmp);
			inst.m_script->decRefCount();
		}
		ResourceManagerHub& rm = m_system.m_engine.getResourceManager();
		inst.m_script = path.isEmpty() ? nullptr : rm.load<JSScript>(path);
		if (inst.m_script) {
			inst.m_script->onLoaded<&ScriptComponent::onScriptLoaded>(&cmp);
		}
	}


	void detectProperties(ScriptInstance& inst, EntityRef entity) {
		duk_context* ctx = m_system.m_global_context;
		duk_push_global_stash(ctx);
		duk_push_pointer(ctx, (void*)inst.m_id);
		duk_get_prop(ctx, -2); //[stash, id] -> [stash, obj]

		duk_enum(ctx, -1, 0);
		u32 valid_properties[256];
		memset(valid_properties, 0, (inst.m_properties.size() + 7) / 8);
		while (duk_next(ctx, -1, 1))
		{
			// [... enum key value]
			if (inst.m_properties.size() > sizeof(valid_properties) * 8) {
				logError("Too many properties in ", inst.m_script->getPath(), ", entity ", entity.index, ". Some will be ignored.");
				inst.m_properties.shrink(sizeof(valid_properties) * 8);
			}

			if (duk_is_function(ctx, -1)) {
				duk_pop_2(ctx);
				continue;
			}

			bool is_entity = false;
			if (duk_is_object(ctx, -1)) {
				// duk_instanceof throws, so we use c_entity to detect entities
				is_entity = duk_get_prop_string(ctx, -1, "c_entity");
				duk_pop(ctx);
				if (!is_entity) {
					duk_pop_2(ctx);
					continue;
				}
			}

			auto& allocator = m_system.m_allocator;
			const char* prop_name = duk_get_string(ctx, -2);
			const StableHash hash(prop_name);
			if (m_property_names.find(hash) < 0)
			{
				m_property_names.emplace(hash, prop_name, allocator);
			}
			int prop_index = ScriptComponent::getProperty(inst, hash);
			if (prop_index >= 0)
			{
				valid_properties[prop_index / 8] |= 1 << (prop_index % 8);
				Property& existing_prop = inst.m_properties[prop_index];
				InputMemoryStream blob(existing_prop.stored_value);
				applyProperty(ctx, inst, existing_prop, blob);
			}
			else
			{
				prop_index = inst.m_properties.size();
				if (prop_index < sizeof(valid_properties) * 8) {
					valid_properties[prop_index / 8] |= 1 << (prop_index % 8);
					auto& prop = inst.m_properties.emplace(allocator);
					switch (duk_get_type(ctx, -1)) {
						case DUK_TYPE_BOOLEAN: prop.type = Property::BOOLEAN; break;
						case DUK_TYPE_STRING: prop.type = Property::STRING; break;
						case DUK_TYPE_NUMBER: prop.type = Property::NUMBER; break;
						default: prop.type = is_entity ? Property::ENTITY : Property::NUMBER; break;
					}
					prop.name_hash = hash;
				}
				else {
					logError("Too many properties in ", inst.m_script->getPath(), ", entity ", entity.index
						, ". Some will be ignored.");
				}
			}
			duk_pop_2(ctx);
		}
		duk_pop_3(ctx); // [stash obj enum] -> []

		for (i32 i = inst.m_properties.size() - 1; i >= 0; --i) {
			if (valid_properties[i / 8] & (1 << (i % 8))) continue;
			inst.m_properties.swapAndPop(i);
		}
	}

	void onScriptLoaded(EntityRef entity, ScriptInstance& instance, bool is_restart) {
		startScript(entity, instance, is_restart);
	}

	void startScript(EntityRef entity, ScriptInstance& instance, bool is_restart) {
		duk_context* ctx = m_system.m_global_context;
		JSWrapper::DebugGuard guard(ctx);

		duk_push_global_stash(ctx);
		duk_push_pointer(ctx, (void*)instance.m_id);

		duk_get_global_string(ctx, "Entity");
		duk_push_pointer(ctx, &m_world);
		JSWrapper::push(ctx, entity.index);
		duk_new(ctx, 2);
		duk_put_global_string(ctx, "_entity");

		duk_push_string(ctx, instance.m_script->getPath().c_str());
		if (duk_pcompile_string_filename(ctx, DUK_COMPILE_EVAL, instance.m_script->getSourceCode()) != 0) {
			const char* error = duk_safe_to_stacktrace(ctx, -1);
			logError(error);
			duk_pop_3(ctx);
			return;
		}

		if (duk_pcall(ctx, 0) != 0) {
			const char* error = duk_safe_to_stacktrace(ctx, -1);
			logError(error);
			duk_pop_3(ctx);
			return;
		}

		if (!duk_is_object(ctx, -1)) {
			duk_pop_3(ctx);
			return;
		}

		duk_put_prop(ctx, -3); // stash[instance.id] = obj

		duk_push_pointer(ctx, (void*)instance.m_id); // [stash, id]
		duk_get_prop(ctx, -2); // [stash, obj]
		if (duk_is_undefined(ctx, -1)) {
			duk_pop_2(ctx);
			return;
		}

		duk_get_prop_string(ctx, -1, "update");
		if (duk_is_callable(ctx, -1)) {
			ContextRef& update = m_updates.emplace();
			update.context = ctx;
			update.id = instance.m_id;
		}
		duk_pop(ctx);

		duk_get_prop_string(ctx, -1, "onInputEvent");
		if (duk_is_callable(ctx, -1)) {
			ContextRef& handler = m_input_handlers.emplace();
			handler.context = ctx;
			handler.id = instance.m_id;
		}
		duk_pop(ctx);

		detectProperties(instance, entity);

		if (!m_scripts_init_called) {
			duk_pop_2(ctx); // stash
			return;
		}

		duk_get_prop_string(ctx, -1, "start");
		if (!duk_is_callable(ctx, -1)) {
			duk_pop_3(ctx);
			return;
		}
		duk_dup(ctx, -2); // [this, func] -> [this, func, this]

		if (duk_pcall_method(ctx, 0)) {
			const char* error = duk_safe_to_string(ctx, -1);
			logError(error);
		}
		duk_pop_2(ctx);
	}


	void startGame() override { m_is_game_running = true; }


	void stopGame() override {
		m_scripts_init_called = false;
		m_is_game_running = false;
		m_updates.clear();
		m_input_handlers.clear();
	}


	void createScript(EntityRef entity) override {
		auto& allocator = m_system.m_allocator;
		ScriptComponent* script = LUMIX_NEW(allocator, ScriptComponent)(*this, entity, allocator);
		m_scripts.insert(entity, script);
		m_world.onComponentCreated(entity, JS_SCRIPT_TYPE, this);
	}

	void destroyScript(EntityRef entity) override {
		auto* script = m_scripts[entity];
		for (auto& scr : script->m_scripts) {
			clearInstance(*script, scr);
			if (scr.m_script) {
				auto& cb = scr.m_script->getObserverCb();
				cb.unbind<&ScriptComponent::onScriptLoaded>(script);
				scr.m_script->decRefCount();
			}
		}
		LUMIX_DELETE(m_system.m_allocator, script);
		m_scripts.erase(entity);
		m_world.onComponentDestroyed(entity, JS_SCRIPT_TYPE, this);
	}

	void serialize(OutputMemoryStream& serializer) override {
		serializer.write(m_scripts.size());
		for (auto iter = m_scripts.begin(), end = m_scripts.end(); iter != end; ++iter) {
			ScriptComponent* script_cmp = iter.value();
			serializer.write(script_cmp->m_entity);
			serializer.write(script_cmp->m_scripts.size());
			for (auto& scr : script_cmp->m_scripts) {
				serializer.writeString(scr.m_script ? scr.m_script->getPath().c_str() : "");
				serializer.write(scr.m_id);
				serializer.write(scr.m_properties.size());
				for (Property& prop : scr.m_properties) {
					serializer.write(prop.name_hash);
					i32 idx = m_property_names.find(prop.name_hash);
					serializer.write(prop.type);
					if (idx >= 0) {
						const char* name = m_property_names.at(idx).c_str();
						switch (prop.type) {
							case Property::BOOLEAN: {
								bool v = getProperty<bool>(prop, name, scr);
								serializer.write(v);
								break;
							}
							case Property::NUMBER: {
								double v = getProperty<double>(prop, name, scr);
								serializer.write(v);
								break;
							}
							case Property::ENTITY: {
								EntityPtr v = getProperty<EntityPtr>(prop, name, scr);
								serializer.write(v);
								break;
							}
							case Property::STRING:
								ASSERT(false);
								// TODO
								break;
						}
					} else {
						ASSERT(false);
						serializer.writeString("");
					}
				}
			}
		}
	}


	void deserialize(InputMemoryStream& serializer, const EntityMap& entity_map, i32 version) override {
		const i32 len = serializer.read<i32>();
		m_scripts.reserve(len + m_scripts.size());
		for (i32 i = 0; i < len; ++i) {
			IAllocator& allocator = m_system.m_allocator;
			
			EntityRef entity;
			serializer.read(entity);
			ScriptComponent* script = LUMIX_NEW(allocator, ScriptComponent)(*this, entity, allocator);

			m_scripts.insert(script->m_entity, script);
			int scr_count;
			serializer.read(scr_count);
			for (i32 scr_idx = 0; scr_idx < scr_count; ++scr_idx) {
				auto& scr = script->m_scripts.emplace(allocator);

				const char* path = serializer.readString();
				serializer.read(scr.m_id);
				i32 num_props;
				serializer.read(num_props);
				scr.m_properties.reserve(num_props);
				for (i32 j = 0; j < num_props; ++j) {
					Property& prop = scr.m_properties.emplace(allocator);
					serializer.read(prop.name_hash);
					serializer.read(prop.type);
					switch (prop.type) {
						case Property::STRING : prop.stored_value.writeString(serializer.readString()); break;
						case Property::NUMBER: prop.stored_value.write(serializer.read<double>()); break;
						case Property::BOOLEAN: prop.stored_value.write(serializer.read<bool>()); break;
						case Property::ENTITY: {
							EntityPtr e = serializer.read<EntityPtr>();
							e = entity_map.get(e);
							prop.stored_value.write(e);
						}
					}
				}
				setScriptPathInternal(*script, scr, Path(path));
			}
			m_world.onComponentCreated(script->m_entity, JS_SCRIPT_TYPE, this);
		}
	}


	ISystem& getSystem() const override { return m_system; }


	void initScripts() {
		ASSERT(!m_scripts_init_called && m_is_game_running);
		// copy m_scripts to tmp, because scripts can create other scripts -> m_scripts is not const
		Array<ScriptComponent*> tmp(m_system.m_allocator);
		tmp.reserve(m_scripts.size());
		for (auto* scr : m_scripts) tmp.push(scr);

		for (auto* scr : tmp) {
			for (int j = 0; j < scr->m_scripts.size(); ++j) {
				auto& instance = scr->m_scripts[j];
				if (!instance.m_script) continue;
				if (!instance.m_script->isReady()) continue;

				auto* call = beginFunctionCall({scr->m_entity.index}, j, "onStartGame");
				if (call) endFunctionCall();
			}
		}
		m_scripts_init_called = true;
	}
	
	void processInputEvent(const ContextRef& ctx_ref, const InputSystem::Event& event) {
		duk_context* ctx = ctx_ref.context;
		JSWrapper::DebugGuard guard(ctx);
		duk_push_object(ctx);
		JSWrapper::setField(ctx, "type", (u32)event.type);
		JSWrapper::setField(ctx, "device_type", (u32)event.device->type);
		JSWrapper::setField(ctx, "device_index", event.device->index);
		
		switch(event.type)
		{
			case InputSystem::Event::DEVICE_ADDED:
				break;
			case InputSystem::Event::DEVICE_REMOVED:
				break;
			case InputSystem::Event::BUTTON:
				JSWrapper::setField(ctx, "down", event.data.button.down);
				JSWrapper::setField(ctx, "key_id", event.data.button.key_id);
				JSWrapper::setField(ctx, "is_repeat", event.data.button.is_repeat);
				JSWrapper::setField(ctx, "x", event.data.button.x);
				JSWrapper::setField(ctx, "y", event.data.button.y);
				break;
			case InputSystem::Event::AXIS:
				JSWrapper::setField(ctx, "x", event.data.axis.x);
				JSWrapper::setField(ctx, "y", event.data.axis.y);
				JSWrapper::setField(ctx, "x_abs", event.data.axis.x_abs);
				JSWrapper::setField(ctx, "y_abs", event.data.axis.y_abs);
				break;
			case InputSystem::Event::TEXT_INPUT:
				JSWrapper::setField(ctx, "text", event.data.text.utf8);
				break;
		}

		duk_push_global_stash(ctx);
		duk_push_pointer(ctx, (void*)ctx_ref.id);
		duk_get_prop(ctx, -2);
		duk_get_prop_string(ctx, -1, "onInputEvent");
		duk_dup(ctx, -2);        //[arg, stash, this, func, this]
		duk_dup(ctx, -5);   //[arg, stash, this, func, this, arg]
		if (duk_pcall_method(ctx, 1) == DUK_EXEC_ERROR) { // [arg, stash, this, retval]
			const char* error = duk_safe_to_string(ctx, -1);
			logError(error);
		}
		duk_pop_n(ctx, 4);
	}

	void processInputEvents() {
		PROFILE_FUNCTION();
		InputSystem& input_system = m_system.m_engine.getInputSystem();
		for (const ContextRef& ctx : m_input_handlers) {
			for (const InputSystem::Event& event : input_system.getEvents()) {
				processInputEvent(ctx, event);
			}
		}
	}

	void update(float time_delta) override {
		PROFILE_FUNCTION();

		if (!m_is_game_running) return;
		if (!m_scripts_init_called) initScripts();

		processInputEvents();
		for (int i = 0; i < m_updates.size(); ++i) {
			ContextRef update_item = m_updates[i];
			duk_push_global_stash(update_item.context);
			duk_push_pointer(update_item.context, (void*)update_item.id);
			duk_get_prop(update_item.context, -2);					//[stash, this]
			duk_get_prop_string(update_item.context, -1, "update"); //[stash, this, func]
			duk_dup(update_item.context, -2);						//[stash, this, func, this]
			duk_push_number(update_item.context, time_delta);
			if (duk_pcall_method(update_item.context, 1) == DUK_EXEC_ERROR) //[stash, this, func, this, arg] -> [stash, this, retval]
			{
				const char* error = duk_safe_to_string(update_item.context, -1);
				logError(error);
			}
			duk_pop_3(update_item.context);
		}
	}


	Property& getScriptProperty(EntityRef entity, int scr_index, const char* name) {
		const StableHash name_hash(name);
		ScriptComponent* script_cmp = m_scripts[entity];
		for (auto& prop : script_cmp->m_scripts[scr_index].m_properties) {
			if (prop.name_hash == name_hash) {
				return prop;
			}
		}

		script_cmp->m_scripts[scr_index].m_properties.emplace(m_system.m_allocator);
		auto& prop = script_cmp->m_scripts[scr_index].m_properties.back();
		prop.name_hash = name_hash;
		return prop;
	}


	Path getScriptPath(EntityRef entity, int scr_index) override {
		auto& tmp = m_scripts[entity]->m_scripts[scr_index];
		return tmp.m_script ? tmp.m_script->getPath() : Path("");
	}


	void setScriptPath(EntityRef entity, int scr_index, const Path& path) override {
		auto* script_cmp = m_scripts[entity];
		if (script_cmp->m_scripts.size() <= scr_index) return;
		setScriptPathInternal(*script_cmp, script_cmp->m_scripts[scr_index], path);
	}


	int getScriptCount(EntityRef entity) override { return m_scripts[entity]->m_scripts.size(); }


	void insertScript(EntityRef entity, int idx) override { m_scripts[entity]->m_scripts.emplaceAt(idx, m_system.m_allocator); }

	uintptr getScriptID(EntityRef entity, i32 scr_index) override {
		return m_scripts[entity]->m_scripts[scr_index].m_id;
	}

	int addScript(EntityRef entity, int scr_index) override {
		ScriptComponent* script_cmp = m_scripts[entity];
		if (scr_index == -1) scr_index = script_cmp->m_scripts.size();
		ScriptInstance& inst = script_cmp->m_scripts.emplaceAt(scr_index, m_system.m_allocator);
		inst.m_id = ++m_id_generator;
		return scr_index;
	}


	void moveScript(EntityRef entity, int scr_index, bool up) override {
		auto* script_cmp = m_scripts[entity];
		if (!up && scr_index > script_cmp->m_scripts.size() - 2) return;
		if (up && scr_index == 0) return;
		int other = up ? scr_index - 1 : scr_index + 1;
		swap(script_cmp->m_scripts[scr_index], script_cmp->m_scripts[other]);
	}


	void removeScript(EntityRef entity, int scr_index) override {
		setScriptPath(entity, scr_index, Path());
		ScriptComponent* script_cmp = m_scripts[entity];
		clearInstance(*script_cmp, script_cmp->m_scripts[scr_index]);
		script_cmp->m_scripts.swapAndPop(scr_index);
	}
		
	template <typename T> static void toString(T val, String& out) {
		char tmp[128];
		toCString(val, Span(tmp));
		out = tmp;
	}

	template <> static void toString(float val, String& out) {
		char tmp[128];
		toCString(val, Span(tmp), 10);
		out = tmp;
	}

	template <> static void toString(Vec3 val, String& out) {
		StaticString<512> tmp("{", val.x, ", ", val.y, ", ", val.z, "}");
		out = tmp;
	}

	template <typename T> T getProperty(Property& prop, const char* prop_name, ScriptInstance& scr) {
		if (!scr.m_script) return {};

		duk_context* ctx = m_system.m_global_context;
		duk_push_global_stash(ctx);
		duk_push_pointer(ctx, (void*)scr.m_id);
		duk_get_prop(ctx, -2);

		duk_get_prop_string(ctx, -1, prop_name);

		if (!JSWrapper::isType<T>(ctx, -1)) {
			duk_pop_3(ctx);
			InputMemoryStream blob(prop.stored_value);
			return blob.read<T>();
		}
		const T res = JSWrapper::toType<T>(ctx, -1);
		duk_pop_3(ctx);
		return res;
	}

	JSScriptSystemImpl& m_system;
	HashMap<EntityRef, ScriptComponent*> m_scripts;
	AssociativeArray<StableHash, String> m_property_names;
	World& m_world;
	Array<ContextRef> m_input_handlers;
	Array<ContextRef> m_updates;
	FunctionCall m_function_call;
	ScriptInstance* m_current_script_instance;
	bool m_scripts_init_called = false;
	bool m_is_api_registered = false;
	bool m_is_game_running = false;
	uintptr m_id_generator = 0;
};

static void js_fatalHandler(void *udata, const char *msg) {
	logError("*** JS FATAL ERROR: ", (msg ? msg : "no message"));
	abort();
}

JSScriptSystemImpl::JSScriptSystemImpl(Engine& engine)
	: m_engine(engine)
	, m_allocator(engine.getAllocator())
	, m_script_manager(m_allocator)
{
	s_instance = this;
	m_script_manager.create(JSScript::TYPE, engine.getResourceManager());

	// TODO allocator
	m_global_context = duk_create_heap(nullptr, nullptr, nullptr, nullptr, js_fatalHandler);

	#include "js_script_system.gen.h"
}

void registerJSAPI(duk_context* ctx);

void JSScriptSystemImpl::initBegin() {
	registerGlobalAPI();
	registerJSAPI(m_global_context);
}

static void convertPropertyToJSName(const char* src, char* out, int max_size) {
	ASSERT(max_size > 0);
	bool to_upper = true;
	char* dest = out;
	while (*src && dest - out < max_size - 1) {
		if (isLetter(*src)) {
			*dest = to_upper && !isUpperCase(*src) ? *src - 'a' + 'A' : *src;
			to_upper = false;
			++dest;
		} else if (isNumeric(*src)) {
			*dest = *src;
			++dest;
		} else {
			to_upper = true;
		}
		++src;
	}
	*dest = 0;
}

template <typename T>
static int JS_getProperty(duk_context* ctx) {
	JSWrapper::DebugGuard guard(ctx, 1);
	duk_push_this(ctx);
	if (duk_is_null_or_undefined(ctx, -1)) {
		duk_eval_error(ctx, "this is null or undefined");
	}
	duk_get_prop_string(ctx, -1, "c_module");
	IModule* module = JSWrapper::toType<IModule*>(ctx, -1);
	if(!module) duk_eval_error(ctx, "getting property on invalid object");

	duk_get_prop_string(ctx, -2, "c_entity");
	EntityRef entity = JSWrapper::toType<EntityRef>(ctx, -1);
	duk_get_prop_string(ctx, -3, "c_cmptype");
	ComponentType cmp_type = { JSWrapper::toType<int>(ctx, -1) };
	duk_pop_3(ctx);

	duk_push_current_function(ctx);
	duk_get_prop_string(ctx, -1, "c_desc");
	const auto* desc = JSWrapper::toType<reflection::Property<T>*>(ctx, -1);
	duk_pop_3(ctx);
	
	ComponentUID cmp;
	cmp.module = module;
	cmp.type = cmp_type;
	cmp.entity = entity;
	const T val = desc->get(cmp, -1);
	JSWrapper::push(ctx, val);

	return 1;
}

template <typename T>
static int JS_setProperty(duk_context* ctx) {
	duk_push_this(ctx);
	if (duk_is_null_or_undefined(ctx, -1))
	{
		duk_eval_error(ctx, "this is null or undefined");
	}
	duk_get_prop_string(ctx, -1, "c_module");
	IModule* module = JSWrapper::toType<IModule*>(ctx, -1);
	if (!module) duk_eval_error(ctx, "getting property on invalid object");

	duk_get_prop_string(ctx, -2, "c_entity");
	EntityRef entity = JSWrapper::toType<EntityRef>(ctx, -1);
	duk_get_prop_string(ctx, -3, "c_cmptype");
	ComponentType cmp_type = { JSWrapper::toType<int>(ctx, -1) };
	duk_pop_3(ctx);

	duk_push_current_function(ctx);
	duk_get_prop_string(ctx, -1, "c_desc");
	auto* desc = JSWrapper::toType<reflection::Property<T>*>(ctx, -1);
	duk_pop(ctx);

	ComponentUID cmp;
	cmp.module = module;
	cmp.type = cmp_type;
	cmp.entity = entity;
	const T v = JSWrapper::toType<T>(ctx, 0);
	desc->set(cmp, -1, v);

	return 0;
}

struct RegisterPropertyVisitor : reflection::IPropertyVisitor {
	template <typename T>
	void reg(const reflection::Property<T>& prop) {
		char tmp[50];
		convertPropertyToJSName(prop.name, tmp, lengthOf(tmp));

		duk_get_global_string(ctx, cmp_type_name);
		if (duk_get_prop_string(ctx, -1, "prototype") != 1) {
			ASSERT(false);
		}

		duk_push_string(ctx, tmp);

		duk_push_c_function(ctx, JS_getProperty<T>, 0);
		JSWrapper::push(ctx, &prop);
		duk_put_prop_string(ctx, -2, "c_desc");

		duk_push_c_function(ctx, JS_setProperty<T>, 1);
		JSWrapper::push(ctx, &prop);
		duk_put_prop_string(ctx, -2, "c_desc");

		duk_def_prop(ctx, -4, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER | DUK_DEFPROP_ENUMERABLE);

		duk_pop_2(ctx);
	}

	void visit(const reflection::Property<float>& prop) override { reg(prop); }
	void visit(const reflection::Property<int>& prop) override { reg(prop); }
	void visit(const reflection::Property<u32>& prop) override { reg(prop); }
	void visit(const reflection::Property<EntityPtr>& prop) override {} // TODO
	void visit(const reflection::Property<Vec2>& prop) override { reg(prop); }
	void visit(const reflection::Property<Vec3>& prop) override { reg(prop); }
	void visit(const reflection::Property<IVec3>& prop) override { reg(prop); }
	void visit(const reflection::Property<Vec4>& prop) override { reg(prop); }
	void visit(const reflection::Property<Path>& prop) override { reg(prop); }
	void visit(const reflection::Property<bool>& prop) override { reg(prop); }
	void visit(const reflection::Property<const char*>& prop) override { reg(prop); }
	void visit(const reflection::ArrayProperty& prop) override {}
	void visit(const reflection::BlobProperty& prop) override {}

	const char* cmp_type_name;
	duk_context* ctx;
};

static void registerComponent(duk_context* ctx, const char* cmp_type_name) {
	auto cmp_type = reflection::getComponentType(cmp_type_name);
	registerJSComponent(ctx, cmp_type, cmp_type_name, &componentJSConstructor);
	
	const reflection::ComponentBase* cmp = reflection::getComponent(cmp_type);
	
	RegisterPropertyVisitor v;
	v.cmp_type_name = cmp_type_name;
	v.ctx = ctx;

	cmp->visit(v);
}

void JSScriptSystemImpl::registerImGuiAPI() {
	duk_context* ctx = m_global_context;
	duk_push_object(ctx);
	duk_dup(ctx, -1);
	duk_put_global_string(ctx, "ImGui");

	#define REGISTER_JS_FUNCTION(F)                                                             \
		duk_push_c_function(ctx, &JSWrapper::wrap<decltype(ImGui::F), &ImGui::F>, DUK_VARARGS); \
		duk_put_prop_string(ctx, -2, #F);

	#define REGISTER_JS_RAW_FUNCTION(F)                     \
		duk_push_c_function(ctx, &JSImGui::F, DUK_VARARGS); \
		duk_put_prop_string(ctx, -2, #F);

		REGISTER_JS_RAW_FUNCTION(Begin);
		REGISTER_JS_FUNCTION(BeginPopup);
		REGISTER_JS_RAW_FUNCTION(Button);
		REGISTER_JS_RAW_FUNCTION(Checkbox);
		REGISTER_JS_RAW_FUNCTION(CollapsingHeader);
		REGISTER_JS_FUNCTION(Columns);
		REGISTER_JS_RAW_FUNCTION(DragFloat);
		REGISTER_JS_FUNCTION(Dummy);
		REGISTER_JS_FUNCTION(End);
		REGISTER_JS_FUNCTION(EndChild);
		REGISTER_JS_FUNCTION(EndPopup);
		REGISTER_JS_FUNCTION(GetColumnWidth);
		REGISTER_JS_FUNCTION(Indent);
		REGISTER_JS_RAW_FUNCTION(LabelText);
		REGISTER_JS_FUNCTION(NewLine);
		REGISTER_JS_FUNCTION(NextColumn);
		REGISTER_JS_RAW_FUNCTION(OpenPopup);
		REGISTER_JS_FUNCTION(PopItemWidth);
		REGISTER_JS_FUNCTION(PopID);
		REGISTER_JS_FUNCTION(PopStyleColor);
		REGISTER_JS_FUNCTION(PopStyleVar);
		REGISTER_JS_FUNCTION(PushItemWidth);
		REGISTER_JS_RAW_FUNCTION(SameLine);
		REGISTER_JS_RAW_FUNCTION(Selectable);
		REGISTER_JS_FUNCTION(Separator);
		REGISTER_JS_RAW_FUNCTION(SliderFloat);
		REGISTER_JS_RAW_FUNCTION(Text);
		REGISTER_JS_FUNCTION(Unindent);

	#undef REGISTER_JS_RAW_FUNCTION
	#undef REGISTER_JS_FUNCTION
}

namespace JSAPI {

int logError(duk_context* ctx) {
	auto* msg = JSWrapper::toType<const char*>(ctx, 0);
	Lumix::logError(msg);
	return 0;
}

int require(duk_context* ctx) {
	auto* path = JSWrapper::toType<const char*>(ctx, 0);
	JSScriptSystemImpl* system = JSScriptSystemImpl::s_instance;

	// TODO cache the object

	ResourceManagerHub& rm = system->m_engine.getResourceManager();
	FileSystem& fs = system->m_engine.getFileSystem();
	OutputMemoryStream content(system->m_allocator);
	if (!fs.getContentSync(Path(path, ".js"), content)) {
		return 0;
	}

	if (duk_pcompile_lstring(ctx, DUK_COMPILE_EVAL, (const char*)content.data(), content.size()) != 0) {
		Lumix::logError("Require failed: ", duk_safe_to_stacktrace(ctx, -1));
		duk_pop(ctx);
		return 0;
	}

	if (duk_pcall(ctx, 0) != 0) {
		duk_pop(ctx);
		return 0;
	}

	// TODO what if there's no return value
	return 1;
}

} // namespace JSAPI

void JSScriptSystemImpl::registerGlobalAPI() {
	registerImGuiAPI();

	registerJSObject(m_global_context, nullptr, "Engine", &ptrJSConstructor);
	registerGlobalVariable(m_global_context, "Engine", "g_engine", &m_engine);

	registerJSObject(m_global_context, nullptr, "World", &ptrJSConstructor);

	registerJSObject(m_global_context, nullptr, "ModuleBase", &ptrJSConstructor);
	registerJSObject(m_global_context, nullptr, "Entity", &entityJSConstructor);

	Span<const reflection::RegisteredComponent> cmps = reflection::getComponents();
	for (const reflection::RegisteredComponent& cmp : cmps) {
		if (!cmp.cmp) continue;
		const char* cmp_type_id = cmp.cmp->name;
		//registerComponent(m_global_context, cmp_type_id);
	}

	duk_context* ctx = m_global_context;
	JSWrapper::DebugGuard guard(ctx);
	duk_push_c_function(ctx, &JSAPI::require, DUK_VARARGS);
	duk_put_global_string(ctx, "require");

	duk_push_object(ctx);
	duk_dup(ctx, -1);
	duk_put_global_string(ctx, "Lumix");
	
	duk_push_c_function(ctx, &JSAPI::logError, DUK_VARARGS);
	duk_put_prop_string(ctx, -2, "logError");

	#define DEF_CONST(T, N) \
		do { duk_push_uint(ctx, (u32)T); duk_put_prop_string(ctx, -2, N); } while(false)

	DEF_CONST(InputSystem::Event::BUTTON, "INPUT_EVENT_BUTTON");
	DEF_CONST(InputSystem::Event::AXIS, "INPUT_EVENT_AXIS");
	DEF_CONST(InputSystem::Event::TEXT_INPUT, "INPUT_EVENT_TEXT_INPUT");

	DEF_CONST(InputSystem::Device::KEYBOARD, "INPUT_DEVICE_KEYBOARD");
	DEF_CONST(InputSystem::Device::MOUSE, "INPUT_DEVICE_MOUSE");
	DEF_CONST(InputSystem::Device::CONTROLLER, "INPUT_DEVICE_CONTROLLER");

	#undef DEF_CONST

	duk_get_global_string(ctx, "Entity");
	duk_push_pointer(ctx, nullptr);
	JSWrapper::push(ctx, INVALID_ENTITY.index);
	duk_new(ctx, 2);
	duk_put_prop_string(ctx, -2, "INVALID_ENTITY");
	duk_pop(ctx);
}

JSScriptSystemImpl::~JSScriptSystemImpl() {
	duk_destroy_heap(m_global_context);
	m_script_manager.destroy();
}


void JSScriptSystemImpl::createModules(World& world) {
	UniquePtr<JSScriptModuleImpl> module = UniquePtr<JSScriptModuleImpl>::create(m_allocator, *this, world);
	world.addModule(module.move());
}


LUMIX_PLUGIN_ENTRY(js) {
	PROFILE_FUNCTION();
	return LUMIX_NEW(engine.getAllocator(), JSScriptSystemImpl)(engine);
}
} // namespace Lumix

#include "js_capi.gen.h"