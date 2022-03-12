#define LUMIX_NO_CUSTOM_CRT
#include "JS_script_system.h"
#include "engine/allocators.h"
#include "engine/array.h"
#include "engine/associative_array.h"
#include "engine/engine.h"
#include "engine/file_system.h"
#include "engine/hash.h"
#include "engine/log.h"
#include "engine/path.h"
#include "engine/plugin.h"
#include "engine/profiler.h"
#include "engine/reflection.h"
#include "engine/resource_manager.h"
#include "engine/stream.h"
#include "engine/string.h"
#include "engine/universe.h"
#include "imgui/imgui.h"
#include "js_script_manager.h"
#include "js_wrapper.h"


namespace Lumix {

static const ComponentType JS_SCRIPT_TYPE = reflection::getComponentType("js_script");

namespace JSImGui {
int Text(duk_context* ctx) {
	auto* text = JSWrapper::checkArg<const char*>(ctx, 0);
	ImGui::Text("%s", text);
	return 0;
}


int OpenPopup(duk_context* ctx) {
	auto* name = JSWrapper::checkArg<const char*>(ctx, 0);
	ImGui::OpenPopup(name);
	return 0;
}


int Button(duk_context* ctx) {
	auto* label = JSWrapper::checkArg<const char*>(ctx, 0);
	bool ret = ImGui::Button(label);
	JSWrapper::push(ctx, ret);
	return 1;
}


int Begin(duk_context* ctx) {
	auto* name = JSWrapper::checkArg<const char*>(ctx, 0);
	bool ret = ImGui::Begin(name);
	JSWrapper::push(ctx, ret);
	return 1;
}


int Checkbox(duk_context* ctx) {
	auto* name = JSWrapper::checkArg<const char*>(ctx, 0);
	bool value = JSWrapper::checkArg<bool>(ctx, 1);
	ImGui::Checkbox(name, &value);
	JSWrapper::push(ctx, value);
	return 1;
}


int CollapsingHeader(duk_context* ctx) {
	auto* name = JSWrapper::checkArg<const char*>(ctx, 0);
	bool ret = ImGui::CollapsingHeader(name);
	JSWrapper::push(ctx, ret);
	return 1;
}


int Selectable(duk_context* ctx) {
	auto* name = JSWrapper::checkArg<const char*>(ctx, 0);
	bool selected = JSWrapper::checkArg<bool>(ctx, 1);
	ImGui::Selectable(name, &selected);
	JSWrapper::push(ctx, selected);
	return 1;
}


int BeginChildFrame(duk_context* ctx) {
	auto* name = JSWrapper::checkArg<const char*>(ctx, 0);
	ImVec2 size(0, 0);
	if (duk_get_top(ctx) > 1) {
		size.x = JSWrapper::checkArg<float>(ctx, 1);
		size.y = JSWrapper::checkArg<float>(ctx, 2);
	}
	bool ret = ImGui::BeginChildFrame(ImGui::GetID(name), size);
	JSWrapper::push(ctx, ret);
	return 1;
}


int SliderFloat(duk_context* ctx) {
	auto* label = JSWrapper::checkArg<const char*>(ctx, 0);
	float value = JSWrapper::checkArg<float>(ctx, 1);
	float v_min = JSWrapper::checkArg<float>(ctx, 2);
	float v_max = JSWrapper::checkArg<float>(ctx, 3);
	ImGui::SliderFloat(label, &value, v_min, v_max);
	JSWrapper::push(ctx, value);
	return 1;
}


int DragFloat(duk_context* ctx) {
	auto* label = JSWrapper::checkArg<const char*>(ctx, 0);
	float value = JSWrapper::checkArg<float>(ctx, 1);
	ImGui::DragFloat(label, &value);
	JSWrapper::push(ctx, value);
	return 1;
}


int SameLine(duk_context* ctx) {
	ImGui::SameLine();
	return 0;
}


int LabelText(duk_context* ctx) {
	auto* label = JSWrapper::checkArg<const char*>(ctx, 0);
	auto* text = JSWrapper::checkArg<const char*>(ctx, 1);
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


static int entityProxyGetter(duk_context* ctx) {
	/*duk_get_prop_string(ctx, 0, "c_universe");
	Universe* universe = (Universe*)duk_get_pointer(ctx, -1);

	duk_get_prop_string(ctx, 0, "c_entity");
	Entity entity = {duk_get_int(ctx, -1)};

	duk_pop_2(ctx);

	const char* cmp_type_name = duk_get_string(ctx, 1);
	ComponentType cmp_type = PropertyRegister::getComponentType(cmp_type_name);

	IScene* scene = universe->getScene(cmp_type);
	if (!scene) return 0;

	EntityRef entity = scene->getComponent(entity, cmp_type);
	if (!cmp.isValid()) return 0;

	duk_get_global_string(ctx, cmp_type_name);
	JSWrapper::push(ctx, scene);
	JSWrapper::push(ctx, cmp);
	duk_new(ctx, 2);*/
	// TODO

	return 1;
}


static int entityJSConstructor(duk_context* ctx) {
	if (!duk_is_constructor_call(ctx)) return DUK_RET_TYPE_ERROR;

	duk_get_global_string(ctx, "Proxy");

	duk_push_this(ctx);

	duk_dup(ctx, 0);
	duk_put_prop_string(ctx, -2, "c_universe");

	duk_dup(ctx, 1);
	duk_put_prop_string(ctx, -2, "c_entity");

	duk_push_object(ctx); // proxy handler
	duk_push_c_function(ctx, entityProxyGetter, 3);
	duk_put_prop_string(ctx, -2, "get");

	duk_new(ctx, 2);

	return 1;
}


static int componentJSConstructor(duk_context* ctx) {
	if (!duk_is_constructor_call(ctx)) return DUK_RET_TYPE_ERROR;
	if (!duk_is_pointer(ctx, 0)) return DUK_RET_TYPE_ERROR;

	duk_push_this(ctx);
	duk_dup(ctx, 0);
	duk_put_prop_string(ctx, -2, "c_scene");

	duk_dup(ctx, 1);
	duk_put_prop_string(ctx, -2, "c_entity");

	duk_push_current_function(ctx);
	duk_get_prop_string(ctx, -1, "c_cmptype");
	duk_put_prop_string(ctx, -3, "c_cmptype");
	duk_pop(ctx);

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

struct JSScriptSystemImpl final : public IPlugin {
	explicit JSScriptSystemImpl(Engine& engine);
	virtual ~JSScriptSystemImpl();

	void serialize(OutputMemoryStream& serializer) const override {}
	bool deserialize(u32 version, InputMemoryStream& serializer) override { return true; }
	u32 getVersion() const override { return 0; }
	void createScenes(Universe& universe) override;
	const char* getName() const override { return "js_script"; }
	JSScriptManager& getScriptManager() { return m_script_manager; }
	void registerGlobalAPI();
	void registerImGuiAPI();
	void reflect();

	Engine& m_engine;
	IAllocator& m_allocator;
	JSScriptManager m_script_manager;
	duk_context* m_global_context;
};


struct JSScriptSceneImpl final : public JSScriptScene {
	struct UpdateData {
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
		ScriptComponent(JSScriptSceneImpl& scene, IAllocator& allocator)
			: m_scripts(allocator)
			, m_scene(scene)
			, m_entity(INVALID_ENTITY) {}


		static int getProperty(ScriptInstance& inst, StableHash32 hash) {
			for (int i = 0, c = inst.m_properties.size(); i < c; ++i) {
				if (inst.m_properties[i].name_hash == hash) return i;
			}
			return -1;
		}


		void onScriptLoaded(Resource::State, Resource::State, Resource& resource) {
			duk_context* ctx = m_scene.m_system.m_global_context;
			for (auto& script : m_scripts) {
				if (!script.m_script) continue;
				if (!script.m_script->isReady()) continue;
				if (script.m_script != &resource) continue;

				m_scene.startScript(m_entity, script, false);
			}
		}


		Array<ScriptInstance> m_scripts;
		JSScriptSceneImpl& m_scene;
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
	JSScriptSceneImpl(JSScriptSystemImpl& system, Universe& ctx)
		: m_system(system)
		, m_universe(ctx)
		, m_scripts(system.m_allocator)
		, m_updates(system.m_allocator)
		, m_property_names(system.m_allocator)
		, m_is_game_running(false)
		, m_is_api_registered(false) {
		m_function_call.is_in_progress = false;

		registerAPI();
	}


	int getVersion() const override { return -1; }


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
		auto* scr = m_scripts[entity];
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
		}
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


	void clear() override {
		Path invalid_path;
		for (auto* script_cmp : m_scripts) {
			if (!script_cmp) continue;

			for (auto& script : script_cmp->m_scripts) {
				setScriptPath(*script_cmp, script, invalid_path);
			}
			LUMIX_DELETE(m_system.m_allocator, script_cmp);
		}
		m_scripts.clear();
	}


	Universe& getUniverse() override { return m_universe; }


	void getInstanceName(IScene& scene, char* out_str, int size) {
		copyString(Span(out_str, size), "g_scene_");
		catString(Span(out_str, size), scene.getPlugin().getName());
	}


	void registerAPI() {
		if (m_is_api_registered) return;
		m_is_api_registered = true;

		duk_context* ctx = m_system.m_global_context;
		registerGlobalVariable(ctx, "Universe", "g_universe", &m_universe);

		Array<UniquePtr<IScene>>& scenes = m_universe.getScenes();
		for (UniquePtr<IScene>& scene : scenes) {
			StaticString<50> type_name(scene->getPlugin().getName(), "_scene");
			char inst_name[50];
			getInstanceName(*scene, inst_name, lengthOf(inst_name));
			registerJSObject(ctx, "SceneBase", type_name, &ptrJSConstructor);
			registerGlobalVariable(ctx, type_name, inst_name, scene.get());
		}
	}


	const char* getPropertyName(StableHash32 name_hash) const {
		int idx = m_property_names.find(name_hash);
		if (idx >= 0) return m_property_names.at(idx).c_str();
		return nullptr;
	}


	void applyProperty(duk_context* ctx, ScriptInstance& script, Property& prop, const char* value) {
		if (!value) return;
		const char* name = getPropertyName(prop.name_hash);
		if (!name) return;

		duk_push_global_stash(ctx);
		duk_push_pointer(ctx, (void*)script.m_id);
		duk_get_prop(ctx, -2);

		if (duk_peval_string(ctx, value) != 0) {
			const char* error = duk_safe_to_string(ctx, -1);
			logError(error);
			duk_pop_3(ctx);
			return;
		}
		duk_put_prop_string(ctx, -2, name);
		duk_pop_2(ctx);
	}


	void setPropertyValue(EntityRef entity, int scr_index, const char* name, const char* value) override {
		ScriptComponent* script_cmp = m_scripts[entity];
		if (!script_cmp) return;
		Property& prop = getScriptProperty(entity, scr_index, name);
		if (!script_cmp->m_scripts[scr_index].m_script->isReady()) {
			prop.stored_value = value;
			return;
		}

		applyProperty(m_system.m_global_context, script_cmp->m_scripts[scr_index], prop, value);
	}

	const char* getPropertyName(EntityRef entity, int scr_index, int index) const {
		auto& script = m_scripts[entity]->m_scripts[scr_index];

		return getPropertyName(script.m_properties[index].name_hash);
	}


	int getPropertyCount(EntityRef entity, int scr_index) const {
		auto& script = m_scripts[entity]->m_scripts[scr_index];

		return script.m_properties.size();
	}


	static void* JSAllocator(void* ud, void* ptr, size_t osize, size_t nsize) {
		auto& allocator = *static_cast<IAllocator*>(ud);
		if (nsize == 0) {
			allocator.deallocate(ptr);
			return nullptr;
		}
		if (nsize > 0 && ptr == nullptr) return allocator.allocate(nsize);

		void* new_mem = allocator.allocate(nsize);
		memcpy(new_mem, ptr, minimum(osize, nsize));
		allocator.deallocate(ptr);
		return new_mem;
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

		duk_context* ctx = m_system.m_global_context;
		duk_push_global_stash(ctx);
		duk_push_pointer(ctx, (void*)inst.m_id);
		duk_del_prop(ctx, -2);
		duk_pop(ctx);

		inst.m_properties.clear();
	}


	void setScriptPath(ScriptComponent& cmp, ScriptInstance& inst, const Path& path) {
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
		while (duk_next(ctx, -1, 1))
		{
			// [... enum key value]
			u32 valid_properties[256];
			if (inst.m_properties.size() > sizeof(valid_properties) * 8) {
				logError("Too many properties in ", inst.m_script->getPath(), ", entity ", entity.index
					, ". Some will be ignored.");
				inst.m_properties.shrink(sizeof(valid_properties) * 8);
			}
			memset(valid_properties, 0, (inst.m_properties.size() + 7) / 8);

			if (duk_is_function(ctx, -1) || duk_is_object(ctx, -1))
			{
				duk_pop_2(ctx);
				continue;
			}

			auto& allocator = m_system.m_allocator;
			const char* prop_name = duk_get_string(ctx, -2);
			const StableHash32 hash(prop_name);
			if (m_property_names.find(hash) < 0)
			{
				m_property_names.emplace(hash, prop_name, allocator);
			}
			int prop_index = ScriptComponent::getProperty(inst, hash);
			if (prop_index >= 0)
			{
				valid_properties[prop_index] = true;
				Property& existing_prop = inst.m_properties[prop_index];
				if (existing_prop.type == Property::ANY)
				{
					switch (duk_get_type(ctx, -1))
					{
						case DUK_TYPE_STRING: existing_prop.type = Property::STRING; break;
						case DUK_TYPE_BOOLEAN: existing_prop.type = Property::BOOLEAN; break;
						default: existing_prop.type = Property::NUMBER;
					}
				}
				applyProperty(ctx, inst, existing_prop, existing_prop.stored_value.c_str());
			}
			else
			{
				prop_index = inst.m_properties.size();
				if (prop_index < sizeof(valid_properties) * 8) {
					valid_properties[prop_index / 8] |= 1 << (prop_index % 8);
					auto& prop = inst.m_properties.emplace(allocator);
					switch (duk_get_type(ctx, -1))
					{
						case DUK_TYPE_BOOLEAN: prop.type = Property::BOOLEAN; break;
						case DUK_TYPE_STRING: prop.type = Property::STRING; break;
						default: prop.type = Property::NUMBER;
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
	}


	void startScript(EntityRef entity, ScriptInstance& instance, bool is_restart) {
		/*duk_context* ctx = m_system.m_global_context;
		duk_push_global_stash(ctx);

		duk_push_pointer(ctx, (void*)instance.m_id);

		duk_get_global_string(ctx, "Entity");
		duk_push_pointer(ctx, &m_universe);
		JSWrapper::push(ctx, entity);
		duk_new(ctx, 2);
		duk_put_global_string(ctx, "_entity");

		if (duk_peval_string(ctx, instance.m_script->getSourceCode()) != 0)
		{
			const char* error = duk_safe_to_string(ctx, -1);
			logError(error);
			duk_pop_3(ctx);
			return;
		}

		if (!duk_is_object(ctx, -1))
		{
			duk_pop_3(ctx);
			return;
		}

		duk_put_prop(ctx, -3); // stash[instance.id] = obj

		duk_push_pointer(ctx, (void*)instance.m_id); // [stash, id]
		duk_get_prop(ctx, -2); // [stash, obj]
		if (duk_is_undefined(ctx, -1))
		{
			duk_pop_2(ctx);
			return;
		}

		duk_get_prop_string(ctx, -1, "update");
		if (duk_is_callable(ctx, -1))
		{
			UpdateData& update = m_updates.emplace();
			update.context = ctx;
			update.id = instance.m_id;
		}
		duk_pop(ctx);

		detectProperties(instance, entity);

		if (!m_scripts_init_called)
		{
			duk_pop_2(ctx); // stash
			return;
		}

		duk_get_prop_string(ctx, -1, "onStartGame");
		if (!duk_is_callable(ctx, -1))
		{
			duk_pop_3(ctx);
			return;
		}
		duk_dup(ctx, -2); // [this, func] -> [this, func, this]

		if (duk_pcall_method(ctx, 0))
		{
			const char* error = duk_safe_to_string(ctx, -1);
			logError(error);
		}
		duk_pop_2(ctx);*/
		ASSERT(false);
		// TODO
	}


	void startGame() override { m_is_game_running = true; }


	void stopGame() override {
		m_scripts_init_called = false;
		m_is_game_running = false;
		m_updates.clear();
	}


	void createScript(EntityRef entity) {
		auto& allocator = m_system.m_allocator;
		ScriptComponent* script = LUMIX_NEW(allocator, ScriptComponent)(*this, allocator);
		script->m_entity = entity;
		m_scripts.insert(entity, script);
		m_universe.onComponentCreated(entity, JS_SCRIPT_TYPE, this);
	}


	void destroyScript(EntityRef entity) {
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
		m_universe.onComponentDestroyed(entity, JS_SCRIPT_TYPE, this);
	}


	void getPropertyValue(EntityRef entity, int scr_index, const char* property_name, char* out, int max_size) override {
		ASSERT(max_size > 0);

		const StableHash32 hash(property_name);
		auto& inst = m_scripts[entity]->m_scripts[scr_index];
		for (auto& prop : inst.m_properties) {
			if (prop.name_hash == hash) {
				if (inst.m_script->isReady())
					getProperty(prop, property_name, inst, out, max_size);
				else
					copyString(Span(out, max_size), prop.stored_value.c_str());
				return;
			}
		}
		*out = '\0';
	}


	void getProperty(Property& prop, const char* prop_name, ScriptInstance& scr, char* out, int max_size) {
		duk_context* ctx = m_system.m_global_context;
		duk_push_global_stash(ctx);
		duk_push_pointer(ctx, (void*)scr.m_id);
		duk_get_prop(ctx, -2);					 // -> [stash obj]
		duk_get_prop_string(ctx, -1, prop_name); // -> [stash obj prop]
		if (duk_is_null_or_undefined(ctx, -1)) {
			copyString(Span(out, max_size), prop.stored_value.c_str());
			duk_pop_3(ctx);
			return;
		}
		switch (prop.type) {
			case Property::BOOLEAN: {
				bool b = duk_get_boolean(ctx, -1) != 0;
				copyString(Span(out, max_size), b ? "true" : "false");
			} break;
			case Property::NUMBER: {
				float val = (float)duk_get_number(ctx, -1);
				toCString(val, Span(out, max_size), 8);
			} break;
			case Property::STRING: {
				copyString(Span(out, max_size), duk_get_string(ctx, -1));
			} break;
			default: ASSERT(false); break;
		}
		duk_pop_3(ctx);
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
					int idx = m_property_names.find(prop.name_hash);
					if (idx >= 0) {
						const char* name = m_property_names.at(idx).c_str();
						char tmp[1024];
						getProperty(prop, name, scr, tmp, lengthOf(tmp));
						serializer.writeString(tmp);
					} else {
						serializer.writeString("");
					}
				}
			}
		}
	}


	void deserialize(InputMemoryStream& serialize, const EntityMap& entity_map, i32 version) override {
		/*int len = serializer.read<int>();
		m_scripts.rehash(len);
		for (int i = 0; i < len; ++i)
		{
			auto& allocator = m_system.m_allocator;
			ScriptComponent* script = LUMIX_NEW(allocator, ScriptComponent)(*this, allocator);

			serializer.read(script->m_entity);
			m_scripts.insert(script->m_entity, script);
			int scr_count;
			serializer.read(scr_count);
			for (int j = 0; j < scr_count; ++j)
			{
				auto& scr = script->m_scripts.emplace(allocator);

				char tmp[MAX_PATH_LENGTH];
				serializer.readString(tmp, MAX_PATH_LENGTH);
				serializer.read(scr.m_id);
				int prop_count;
				serializer.read(prop_count);
				scr.m_properties.reserve(prop_count);
				for (int j = 0; j < prop_count; ++j)
				{
					Property& prop = scr.m_properties.emplace(allocator);
					prop.type = Property::ANY;
					serializer.read(prop.name_hash);
					char tmp[1024];
					tmp[0] = 0;
					serializer.readString(tmp, sizeof(tmp));
					prop.stored_value = tmp;
				}
				setScriptPath(*script, scr, Path(tmp));
			}
			EntityRef entity = { script->m_entity.index };
			m_universe.addComponent(script->m_entity, JS_SCRIPT_TYPE, this, cmp);
		}*/
		// TODO
		ASSERT(false);
	}


	IPlugin& getPlugin() const override { return m_system; }


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


	void update(float time_delta, bool paused) override {
		PROFILE_FUNCTION();

		if (m_is_game_running && !m_scripts_init_called) initScripts();

		if (paused || !m_is_game_running) return;

		for (int i = 0; i < m_updates.size(); ++i) {
			UpdateData update_item = m_updates[i];
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
		const StableHash32 name_hash(name);
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
		setScriptPath(*script_cmp, script_cmp->m_scripts[scr_index], path);
	}


	int getScriptCount(EntityRef entity) override { return m_scripts[entity]->m_scripts.size(); }


	void insertScript(EntityRef entity, int idx) override { m_scripts[entity]->m_scripts.emplaceAt(idx, m_system.m_allocator); }


	int addScript(EntityRef entity) override {
		ScriptComponent* script_cmp = m_scripts[entity];
		ScriptInstance& inst = script_cmp->m_scripts.emplace(m_system.m_allocator);
		inst.m_id = ++m_id_generator;
		return script_cmp->m_scripts.size() - 1;
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


	JSScriptSystemImpl& m_system;
	HashMap<EntityRef, ScriptComponent*> m_scripts;
	AssociativeArray<StableHash32, String> m_property_names;
	Universe& m_universe;
	Array<UpdateData> m_updates;
	FunctionCall m_function_call;
	ScriptInstance* m_current_script_instance;
	bool m_scripts_init_called = false;
	bool m_is_api_registered = false;
	bool m_is_game_running = false;
	uintptr m_id_generator = 0;
};


JSScriptSystemImpl::JSScriptSystemImpl(Engine& engine)
	: m_engine(engine)
	, m_allocator(engine.getAllocator())
	, m_script_manager(m_allocator) {
	m_script_manager.create(JSScript::TYPE, engine.getResourceManager());

	m_global_context = duk_create_heap_default();
	registerGlobalAPI();

	LUMIX_SCENE(JSScriptSceneImpl, "js_script"

	);
}

void JSScriptSystemImpl::reflect() {}

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
	duk_push_this(ctx);
	if (duk_is_null_or_undefined(ctx, -1)) {
		duk_eval_error(ctx, "this is null or undefined");
	}
	duk_get_prop_string(ctx, -1, "c_scene");
	IScene* scene = JSWrapper::toType<IScene*>(ctx, -1);
	if(!scene) duk_eval_error(ctx, "getting property on invalid object");

	duk_get_prop_string(ctx, -1, "c_entity");
	EntityRef entity = JSWrapper::toType<EntityRef>(ctx, -1);
	duk_get_prop_string(ctx, -1, "c_cmptype");
	ComponentType cmp_type = { JSWrapper::toType<int>(ctx, -1) };
	duk_pop(ctx);

	duk_push_current_function(ctx);
	duk_get_prop_string(ctx, -1, "c_desc");
	const auto* desc = JSWrapper::toType<reflection::Property<T>*>(ctx, -1);
	duk_pop(ctx);
	
	ComponentUID cmp;
	cmp.scene = scene;
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
	duk_get_prop_string(ctx, -1, "c_scene");
	IScene* scene = JSWrapper::toType<IScene*>(ctx, -1);
	if (!scene) duk_eval_error(ctx, "getting property on invalid object");

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
	cmp.scene = scene;
	cmp.type = cmp_type;
	cmp.entity = entity;
	const T v = JSWrapper::checkArg<T>(ctx, 0);
	desc->set(cmp, -1, v);

	return 0;
}


#define REGISTER_JS_METHOD(O, F)                                    \
	do {                                                            \
		auto f = &JSWrapper::wrapMethod<O, decltype(&O::F), &O::F>; \
		registerMethod(m_global_context, #O, #F, f);                \
	} while (false)


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
	void visit(const reflection::DynamicProperties& prop) override {}
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
		REGISTER_JS_RAW_FUNCTION(BeginChildFrame);
		REGISTER_JS_FUNCTION(BeginPopup);
		REGISTER_JS_RAW_FUNCTION(Button);
		REGISTER_JS_RAW_FUNCTION(Checkbox);
		REGISTER_JS_RAW_FUNCTION(CollapsingHeader);
		REGISTER_JS_FUNCTION(Columns);
		REGISTER_JS_RAW_FUNCTION(DragFloat);
		REGISTER_JS_FUNCTION(Dummy);
		REGISTER_JS_FUNCTION(End);
		REGISTER_JS_FUNCTION(EndChildFrame);
		REGISTER_JS_FUNCTION(EndPopup);
		REGISTER_JS_FUNCTION(GetColumnWidth);
		REGISTER_JS_FUNCTION(Image);
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


void JSScriptSystemImpl::registerGlobalAPI() {
	registerImGuiAPI();

	registerJSObject(m_global_context, nullptr, "Engine", &ptrJSConstructor);
	registerGlobalVariable(m_global_context, "Engine", "g_engine", &m_engine);

	registerJSObject(m_global_context, nullptr, "Universe", &ptrJSConstructor);

	registerJSObject(m_global_context, nullptr, "SceneBase", &ptrJSConstructor);
	registerJSObject(m_global_context, nullptr, "Entity", &entityJSConstructor);

	const u32 count = reflection::getComponents().length();
	for (u32 i = 0; i < count; ++i) {
		const char* cmp_type_id = reflection::getComponents()[i].cmp->name;
		registerComponent(m_global_context, cmp_type_id);
	}
}

JSScriptSystemImpl::~JSScriptSystemImpl() {
	duk_destroy_heap(m_global_context);
	m_script_manager.destroy();
}


void JSScriptSystemImpl::createScenes(Universe& ctx) {
	UniquePtr<JSScriptSceneImpl> scene = UniquePtr<JSScriptSceneImpl>::create(m_allocator, *this, ctx);
	ctx.addScene(scene.move());
}


LUMIX_PLUGIN_ENTRY(js) {
	return LUMIX_NEW(engine.getAllocator(), JSScriptSystemImpl)(engine);
}
} // namespace Lumix
