#include "JS_script_system.h"
#include "engine/array.h"
#include "engine/base_proxy_allocator.h"
#include "engine/binary_array.h"
#include "engine/blob.h"
#include "engine/crc32.h"
#include "engine/debug/debug.h"
#include "engine/engine.h"
#include "engine/fs/file_system.h"
#include "engine/iallocator.h"
#include "engine/iplugin.h"
#include "engine/json_serializer.h"
#include "engine/log.h"
#include "engine/path_utils.h"
#include "engine/plugin_manager.h"
#include "engine/profiler.h"
#include "engine/property_descriptor.h"
#include "engine/property_register.h"
#include "engine/resource_manager.h"
#include "engine/serializer.h"
#include "engine/string.h"
#include "engine/universe/universe.h"
#include "js_script_manager.h"
#include "js_wrapper.h"


namespace Lumix
{
	static const ComponentType JS_SCRIPT_TYPE = PropertyRegister::getComponentType("js_script");
	static const ResourceType JS_SCRIPT_RESOURCE_TYPE("js_script");


	enum class JSSceneVersion : int
	{
		PROPERTY_TYPE,

		LATEST
	};


	static int ptrJSConstructor(duk_context* ctx)
	{
		if (!duk_is_constructor_call(ctx)) return DUK_RET_TYPE_ERROR;

		duk_push_this(ctx);
		duk_dup(ctx, 0);
		duk_put_prop_string(ctx, -2, "c_ptr");

		return 0;
	}

	
	static int entityProxyGetter(duk_context* ctx)
	{
		duk_get_prop_string(ctx, 0, "c_universe");
		Universe* universe = (Universe*)duk_to_pointer(ctx, -1);

		duk_get_prop_string(ctx, 0, "c_entity");
		Entity entity = {duk_to_int(ctx, -1)};

		duk_pop_2(ctx);

		const char* cmp_type_name = duk_to_string(ctx, 1);
		ComponentType cmp_type = PropertyRegister::getComponentType(cmp_type_name);

		IScene* scene = universe->getScene(cmp_type);
		if (!scene) return 0;

		ComponentHandle cmp = scene->getComponent(entity, cmp_type);
		if (!cmp.isValid()) return 0;

		duk_get_global_string(ctx, cmp_type_name);
		JSWrapper::push(ctx, scene);
		JSWrapper::push(ctx, cmp);
		duk_new(ctx, 2);

		return 1;
	}


	static int entityJSConstructor(duk_context* ctx)
	{
		if (!duk_is_constructor_call(ctx)) return DUK_RET_TYPE_ERROR;

		duk_get_global_string(ctx, "Proxy");

		duk_push_this(ctx);

		duk_dup(ctx, 0);
		duk_put_prop_string(ctx, -2, "c_universe");

		duk_dup(ctx, 1);
		duk_put_prop_string(ctx, -2, "c_entity");

		duk_push_object(ctx); //proxy handler
		duk_push_c_function(ctx, entityProxyGetter, 3);
		duk_put_prop_string(ctx, -2, "get");

		duk_new(ctx, 2);

		return 1;
	}


	static int componentJSConstructor(duk_context* ctx)
	{
		duk_get_global_string(ctx, "camera");
		auto xx = duk_get_type(ctx, -1);
		duk_pop(ctx);

		if (!duk_is_constructor_call(ctx)) return DUK_RET_TYPE_ERROR;
		if (!duk_is_pointer(ctx, 0)) return DUK_RET_TYPE_ERROR;

		duk_push_this(ctx);
		duk_dup(ctx, 0);
		duk_put_prop_string(ctx, -2, "c_scene");

		duk_dup(ctx, 1);
		duk_put_prop_string(ctx, -2, "c_cmphandle");

		duk_push_current_function(ctx);
		duk_get_prop_string(ctx, -1, "c_cmptype");
		duk_put_prop_string(ctx, -3, "c_cmptype");
		duk_pop(ctx);

		return 0;
	}


	static void registerJSObject(duk_context* ctx, const char* prototype, const char* name, duk_c_function constructor)
	{
		duk_push_c_function(ctx, constructor, DUK_VARARGS);

		if (prototype == nullptr || prototype[0] == '\0')
		{
			duk_push_object(ctx); // prototype
			duk_put_prop_string(ctx, -2, "prototype");
		}
		else
		{
			duk_get_global_string(ctx, prototype);
			duk_set_prototype(ctx, -2);
		}

		duk_put_global_string(ctx, name);
	}


	static void registerJSComponent(duk_context* ctx,
		ComponentType cmp_type,
		const char* name,
		duk_c_function constructor)
	{
		duk_push_c_function(ctx, constructor, DUK_VARARGS);

		duk_push_object(ctx); // prototype
		duk_put_prop_string(ctx, -2, "prototype");

		duk_push_int(ctx, cmp_type.index);
		duk_put_prop_string(ctx, -2, "c_cmptype");

		duk_put_global_string(ctx, name);
	}


	static void registerMethod(duk_context* ctx, const char* obj, const char* method_name, duk_c_function method)
	{
		if (duk_get_global_string(ctx, obj) == 0)
		{
			ASSERT(false);
			return;
		}

		if (duk_get_prop_string(ctx, -1, "prototype") != 1)
		{
			ASSERT(false);
			return;
		}
		duk_push_string(ctx, method_name);
		duk_push_c_function(ctx, method, DUK_VARARGS);
		duk_put_prop(ctx, -3);
		duk_pop_2(ctx);
	}


	static void registerGlobal(duk_context* ctx, const char* type_name, const char* var_name, void* ptr)
	{
		if (duk_get_global_string(ctx, type_name) != 1)
		{
			ASSERT(false);
			return;
		}
		duk_push_pointer(ctx, ptr);
		duk_new(ctx, 1);
		duk_put_global_string(ctx, var_name);
	}


	struct JSScriptSystemImpl LUMIX_FINAL : public IPlugin
	{
		explicit JSScriptSystemImpl(Engine& engine);
		virtual ~JSScriptSystemImpl();

		void createScenes(Universe& universe) override;
		void destroyScene(IScene* scene) override;
		const char* getName() const override { return "js_script"; }
		JSScriptManager& getScriptManager() { return m_script_manager; }
		void registerGlobalAPI();

		Engine& m_engine;
		Debug::Allocator m_allocator;
		JSScriptManager m_script_manager;
		duk_context* m_global_context;
	};


	struct JSScriptSceneImpl LUMIX_FINAL : public JSScriptScene
	{
		struct TimerData
		{
			float time;
			duk_context* context;
			int func;
		};

		struct UpdateData
		{
			JSScript* script;
			duk_context* context;
			int environment;
		};


		struct ScriptInstance
		{
			explicit ScriptInstance(IAllocator& allocator)
				: m_properties(allocator)
				, m_script(nullptr)
				, m_environment(-1)
				, m_thread_ref(-1)
			{
			}

			JSScript* m_script;
			duk_context* m_context;
			int m_environment;
			int m_thread_ref;
			Array<Property> m_properties;
		};


		struct ScriptComponent
		{
			ScriptComponent(JSScriptSceneImpl& scene, IAllocator& allocator)
				: m_scripts(allocator)
				, m_scene(scene)
				, m_entity(INVALID_ENTITY)
			{
			}


			static int getProperty(ScriptInstance& inst, u32 hash)
			{
				for(int i = 0, c = inst.m_properties.size(); i < c; ++i)
				{
					if (inst.m_properties[i].name_hash == hash) return i;
				}
				return -1;
			}


			void detectProperties(ScriptInstance& inst)
			{
				// TODO
				/*static const u32 INDEX_HASH = crc32("__index");
				static const u32 THIS_HASH = crc32("this");
				JS_State* L = inst.m_state;
				bool is_env_valid = JS_rawgeti(L, JS_REGISTRYINDEX, inst.m_environment) == JS_TTABLE;
				ASSERT(is_env_valid);
				JS_pushnil(L);
				auto& allocator = m_scene.m_system.m_allocator;
				BinaryArray valid_properties(m_scene.m_system.m_engine.getLIFOAllocator());
				valid_properties.resize(inst.m_properties.size());
				setMemory(valid_properties.getRaw(), 0, valid_properties.size() >> 3);

				while (JS_next(L, -2))
				{
					if (JS_type(L, -1) != JS_TFUNCTION)
					{
						const char* name = JS_tostring(L, -2);
						if(name[0] != '_')
						{
							u32 hash = crc32(name);
							if (m_scene.m_property_names.find(hash) < 0)
							{
								m_scene.m_property_names.emplace(hash, name, allocator);
							}
							if (hash != INDEX_HASH && hash != THIS_HASH)
							{
								int prop_index = getProperty(inst, hash);
								if (prop_index >= 0)
								{
									valid_properties[prop_index] = true;
									Property& existing_prop = inst.m_properties[prop_index];
									if (existing_prop.type == Property::ANY)
									{
										switch (JS_type(inst.m_state, -1))
										{
										case JS_TSTRING: existing_prop.type = Property::STRING; break;
										case JS_TBOOLEAN: existing_prop.type = Property::BOOLEAN; break;
										default: existing_prop.type = Property::FLOAT;
										}
									}
									m_scene.applyProperty(inst, existing_prop, existing_prop.stored_value.c_str());
								}
								else
								{
									auto& prop = inst.m_properties.emplace(allocator);
									valid_properties.push(true);
									switch (JS_type(inst.m_state, -1))
									{
									case JS_TBOOLEAN: prop.type = Property::BOOLEAN; break;
									case JS_TSTRING: prop.type = Property::STRING; break;
									default: prop.type = Property::FLOAT;
									}
									prop.name_hash = hash;
								}
							}
						}
					}
					JS_pop(L, 1);
				}
				for (int i = inst.m_properties.size() - 1; i >= 0; --i)
				{
					if (valid_properties[i]) continue;
					inst.m_properties.eraseFast(i);
				}*/
			}


			void onScriptLoaded(Resource::State, Resource::State, Resource& resource)
			{
				/*JS_State* L = m_scene.m_system.m_engine.getState();
				for (auto& script : m_scripts)
				{
					if (!script.m_script) continue;
					if (!script.m_script->isReady()) continue;
					if (script.m_script != &resource) continue;

					bool is_reload = true;
					if (!script.m_state)
					{
						is_reload = false;
						script.m_environment = -1;

						script.m_state = JS_newthread(L);
						JS_pushvalue(L, -1);
						script.m_thread_ref = JSL_ref(L, JS_REGISTRYINDEX);
						JS_pop(L, 1);
						JS_newtable(script.m_state);
						// reference environment
						JS_pushvalue(script.m_state, -1);
						script.m_environment = JSL_ref(script.m_state, JS_REGISTRYINDEX);

						// environment's metatable & __index
						JS_pushvalue(script.m_state, -1);
						JS_setmetatable(script.m_state, -2);
						JS_pushglobaltable(script.m_state);
						JS_setfield(script.m_state, -2, "__index");

						// set this
						JS_pushinteger(script.m_state, m_entity.index);
						JS_setfield(script.m_state, -2, "this");
					}

					JS_rawgeti(script.m_state, JS_REGISTRYINDEX, script.m_environment);
					bool errors = JSL_loadbuffer(script.m_state,
						script.m_script->getSourceCode(),
						stringLength(script.m_script->getSourceCode()),
						script.m_script->getPath().c_str()) != JS_OK;

					if (errors)
					{
						g_log_error.log("JS Script") << script.m_script->getPath() << ": "
							<< JS_tostring(script.m_state, -1);
						JS_pop(script.m_state, 1);
						continue;
					}

					JS_pushvalue(script.m_state, -2);
					JS_setupvalue(script.m_state, -2, 1); // function's environment

					m_scene.m_current_script_instance = &script;
					errors = errors || JS_pcall(script.m_state, 0, 0, 0) != JS_OK;
					if (errors)
					{
						g_log_error.log("JS Script") << script.m_script->getPath() << ": "
							<< JS_tostring(script.m_state, -1);
						JS_pop(script.m_state, 1);
					}
					JS_pop(script.m_state, 1);

					detectProperties(script);

					if (m_scene.m_is_game_running) m_scene.startScript(script, is_reload);
				}*/
				// TODO
			}


			Array<ScriptInstance> m_scripts;
			JSScriptSceneImpl& m_scene;
			Entity m_entity;
		};


		struct FunctionCall : IFunctionCall
		{
			void add(int parameter) override
			{
				// TODO
				// JS_pushinteger(state, parameter);
				++parameter_count;
			}


			void add(float parameter) override
			{
				// TODO
				// JS_pushnumber(state, parameter);
				++parameter_count;
			}


			void add(void* parameter) override
			{
				// TODO
				// JS_pushlightuserdata(state, parameter);
				++parameter_count;
			}


			void addEnvironment(int env) override
			{
				// TODO
				// bool is_valid = JS_rawgeti(state, JS_REGISTRYINDEX, env) == JS_TTABLE;
				//ASSERT(is_valid);
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
			, m_timers(system.m_allocator)
			, m_property_names(system.m_allocator)
			, m_is_game_running(false)
			, m_is_api_registered(false)
		{
			m_function_call.is_in_progress = false;
			
			registerAPI();
			ctx.registerComponentType(JS_SCRIPT_TYPE, this, &JSScriptSceneImpl::serializeJSScript, &JSScriptSceneImpl::deserializeJSScript);
		}


		int getVersion() const override { return (int)JSSceneVersion::LATEST; }


		ComponentHandle getComponent(Entity entity) override
		{
			if (m_scripts.find(entity) == m_scripts.end()) return INVALID_COMPONENT;
			return {entity.index};
		}


		IFunctionCall* beginFunctionCall(ComponentHandle cmp, int scr_index, const char* function) override
		{
			// TODO
			// 
/*			ASSERT(!m_function_call.is_in_progress);

			auto* script_cmp = m_scripts[{cmp.index}];
			auto& script = script_cmp->m_scripts[scr_index];
			if (!script.m_state) return nullptr;

			bool is_env_valid = JS_rawgeti(script.m_state, JS_REGISTRYINDEX, script.m_environment) == JS_TTABLE;
			ASSERT(is_env_valid);
			if (JS_getfield(script.m_state, -1, function) != JS_TFUNCTION)
			{
				JS_pop(script.m_state, 2);
				return nullptr;
			}

			m_function_call.state = script.m_state;
			m_function_call.cmp = script_cmp;
			m_function_call.is_in_progress = true;
			m_function_call.parameter_count = 0;
			m_function_call.scr_index = scr_index;

			return &m_function_call;*/
			return nullptr;
		}


		void endFunctionCall() override
		{
			// TODO
			/*ASSERT(m_function_call.is_in_progress);

			m_function_call.is_in_progress = false;

			auto& script = m_function_call.cmp->m_scripts[m_function_call.scr_index];
			if (!script.m_state) return;

			if (JS_pcall(script.m_state, m_function_call.parameter_count, 0, 0) != JS_OK)
			{
				g_log_warning.log("JS Script") << JS_tostring(script.m_state, -1);
				JS_pop(script.m_state, 1);
			}
			JS_pop(script.m_state, 1);*/
		}


		int getPropertyCount(ComponentHandle cmp, int scr_index) override
		{
			return m_scripts[{cmp.index}]->m_scripts[scr_index].m_properties.size();
		}


		const char* getPropertyName(ComponentHandle cmp, int scr_index, int prop_index) override
		{
			return getPropertyName(m_scripts[{cmp.index}]->m_scripts[scr_index].m_properties[prop_index].name_hash);
		}


		ResourceType getPropertyResourceType(ComponentHandle cmp, int scr_index, int prop_index) override
		{
			return m_scripts[{cmp.index}]->m_scripts[scr_index].m_properties[prop_index].resource_type;
		}


		Property::Type getPropertyType(ComponentHandle cmp, int scr_index, int prop_index) override
		{
			return m_scripts[{cmp.index}]->m_scripts[scr_index].m_properties[prop_index].type;
		}


		void getScriptData(ComponentHandle cmp, OutputBlob& blob) override
		{
			auto* scr = m_scripts[{cmp.index}];
			blob.write(scr->m_scripts.size());
			for (int i = 0; i < scr->m_scripts.size(); ++i)
			{
				auto& inst = scr->m_scripts[i];
				blob.writeString(inst.m_script ? inst.m_script->getPath().c_str() : "");
				blob.write(inst.m_properties.size());
				for (auto& prop : inst.m_properties)
				{
					blob.write(prop.name_hash);
					blob.write(prop.type);
					char tmp[1024];
					tmp[0] = '\0';
					const char* prop_name = getPropertyName(prop.name_hash);
					if(prop_name) getPropertyValue(cmp, i, getPropertyName(prop.name_hash), tmp, lengthOf(tmp));
					blob.writeString(prop_name ? tmp : prop.stored_value.c_str());
				}
			}
		}


		duk_context* getGlobalContext() override
		{
			return m_system.m_global_context;
		}


		void setScriptData(ComponentHandle cmp, InputBlob& blob) override
		{
			// TODO
			/*
			auto* scr = m_scripts[{cmp.index}];
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


		void clear() override
		{
			Path invalid_path;
			for (auto* script_cmp : m_scripts)
			{
				if (!script_cmp) continue;

				for (auto script : script_cmp->m_scripts)
				{
					setScriptPath(*script_cmp, script, invalid_path);
				}
				LUMIX_DELETE(m_system.m_allocator, script_cmp);
			}
			m_scripts.clear();
		}


		duk_context* getContext(ComponentHandle cmp, int scr_index) override
		{
			return m_scripts[{cmp.index}]->m_scripts[scr_index].m_context;
		}


		Universe& getUniverse() override { return m_universe; }

		// TODO
		/*
		static int setPropertyType(JS_State* L)
		{
			const char* prop_name = JSWrapper::checkArg<const char*>(L, 1);
			int type = JSWrapper::checkArg<int>(L, 2);
			ResourceType resource_type;
			if (type == Property::Type::RESOURCE)
			{
				resource_type = ResourceType(JSWrapper::checkArg<const char*>(L, 3));
			}
			int tmp = JS_getglobal(L, "g_scene_JS_script");
			ASSERT(tmp == JS_TLIGHTUSERDATA);
			auto* scene = JSWrapper::toType<JSScriptSceneImpl*>(L, -1);
			u32 prop_name_hash = crc32(prop_name);
			for (auto& prop : scene->m_current_script_instance->m_properties)
			{
				if (prop.name_hash == prop_name_hash)
				{
					prop.type = (Property::Type)type;
					prop.resource_type = resource_type;
					JS_pop(L, -1);
					return 0;
				}
			}

			auto& prop = scene->m_current_script_instance->m_properties.emplace(scene->m_system.m_allocator);
			prop.name_hash = prop_name_hash;
			prop.type = (Property::Type)type;
			prop.resource_type = resource_type;
			if (scene->m_property_names.find(prop_name_hash) < 0)
			{
				scene->m_property_names.emplace(prop_name_hash, prop_name, scene->m_system.m_allocator);
			}
			return 0;
		}
		*/

		void registerPropertyAPI()
		{
			// TODO
			/*JS_State* L = m_system.m_engine.getState();
			auto f = &JSWrapper::wrap<decltype(&setPropertyType), &setPropertyType>;
			JSWrapper::createSystemFunction(L, "Editor", "setPropertyType", f);
			JSWrapper::createSystemVariable(L, "Editor", "BOOLEAN_PROPERTY", Property::BOOLEAN);
			JSWrapper::createSystemVariable(L, "Editor", "FLOAT_PROPERTY", Property::FLOAT);
			JSWrapper::createSystemVariable(L, "Editor", "ENTITY_PROPERTY", Property::ENTITY);
			JSWrapper::createSystemVariable(L, "Editor", "RESOURCE_PROPERTY", Property::RESOURCE);*/
		}

		// TODO
		/*
		static int getEnvironment(JS_State* L)
		{
			auto* scene = JSWrapper::checkArg<JSScriptScene*>(L, 1);
			Entity entity = JSWrapper::checkArg<Entity>(L, 2);
			int scr_index = JSWrapper::checkArg<int>(L, 3);

			ComponentHandle cmp = scene->getComponent(entity);
			if (cmp == INVALID_COMPONENT)
			{
				JS_pushnil(L);
				return 1;
			}
			int count = scene->getScriptCount(cmp);
			if (scr_index >= count)
			{
				JS_pushnil(L);
				return 1;
			}

			int env = scene->getEnvironment(cmp, scr_index);
			if (env < 0)
			{
				JS_pushnil(L);
			}
			else
			{
				bool is_valid = JS_rawgeti(L, JS_REGISTRYINDEX, env) == JS_TTABLE;
				ASSERT(is_valid);
			}
			return 1;
		}


		static int JS_getProperty(JS_State* L)
		{
			auto* desc = JSWrapper::toType<PropertyDescriptorBase*>(L, JS_upvalueindex(1));
			ComponentType type = { JSWrapper::toType<int>(L, JS_upvalueindex(2)) };
			ComponentUID cmp;
			cmp.scene = JSWrapper::checkArg<IScene*>(L, 1);
			cmp.handle = JSWrapper::checkArg<ComponentHandle>(L, 2);
			cmp.type = type;
			cmp.entity = INVALID_ENTITY;
			switch (desc->getType())
			{
				case PropertyDescriptorBase::DECIMAL:
				{
					float v;
					OutputBlob blob(&v, sizeof(v));
					desc->get(cmp, -1, blob);
					JSWrapper::push(L, v);
				}
				break;
				case PropertyDescriptorBase::BOOL:
				{
					bool v;
					OutputBlob blob(&v, sizeof(v));
					desc->get(cmp, -1, blob);
					JSWrapper::push(L, v);
				}
				break;
				case PropertyDescriptorBase::INTEGER:
				{
					int v;
					OutputBlob blob(&v, sizeof(v));
					desc->get(cmp, -1, blob);
					JSWrapper::push(L, v);
				}
				break;
				case PropertyDescriptorBase::RESOURCE:
				case PropertyDescriptorBase::FILE:
				case PropertyDescriptorBase::STRING:
				{
					char buf[1024];
					OutputBlob blob(buf, sizeof(buf));
					desc->get(cmp, -1, blob);
					JSWrapper::push(L, buf);
				}
				break;
				case PropertyDescriptorBase::COLOR:
				case PropertyDescriptorBase::VEC3:
				{
					Vec3 v;
					OutputBlob blob(&v, sizeof(v));
					desc->get(cmp, -1, blob);
					JSWrapper::push(L, v);
				}
				break;
				case PropertyDescriptorBase::VEC2:
				{
					Vec2 v;
					OutputBlob blob(&v, sizeof(v));
					desc->get(cmp, -1, blob);
					JSWrapper::push(L, v);
				}
				break;
				case PropertyDescriptorBase::INT2:
				{
					Int2 v;
					OutputBlob blob(&v, sizeof(v));
					desc->get(cmp, -1, blob);
					JSWrapper::push(L, v);
				}
				break;
				case PropertyDescriptorBase::ENTITY:
				{
					Entity v;
					OutputBlob blob(&v, sizeof(v));
					desc->get(cmp, -1, blob);
					JSWrapper::push(L, v);
				}
				break;
				case PropertyDescriptorBase::ENUM:
				{
					int v;
					OutputBlob blob(&v, sizeof(v));
					desc->get(cmp, -1, blob);
					JSWrapper::push(L, v);
				}
				break;
				default: JSL_argerror(L, 1, "Unsupported property type"); break;
			}
			return 1;
		}


		static int JS_setProperty(JS_State* L)
		{
			auto* desc = JSWrapper::toType<PropertyDescriptorBase*>(L, JS_upvalueindex(1));
			ComponentType type = { JSWrapper::toType<int>(L, JS_upvalueindex(2)) };
			ComponentUID cmp;
			cmp.scene = JSWrapper::checkArg<IScene*>(L, 1);
			cmp.handle = JSWrapper::checkArg<ComponentHandle>(L, 2);
			cmp.type = type;
			cmp.entity = INVALID_ENTITY;
			switch(desc->getType())
			{
				case PropertyDescriptorBase::DECIMAL:
				{
					auto v = JSWrapper::checkArg<float>(L, 3);
					InputBlob blob(&v, sizeof(v));
					desc->set(cmp, -1, blob);
				}
				break;
				case PropertyDescriptorBase::INTEGER:
				{
					auto v = JSWrapper::checkArg<int>(L, 3);
					InputBlob blob(&v, sizeof(v));
					desc->set(cmp, -1, blob);
				}
				break;
				case PropertyDescriptorBase::BOOL:
				{
					auto v = JSWrapper::checkArg<bool>(L, 3);
					InputBlob blob(&v, sizeof(v));
					desc->set(cmp, -1, blob);
				}
				break;
				case PropertyDescriptorBase::RESOURCE:
				case PropertyDescriptorBase::FILE:
				case PropertyDescriptorBase::STRING:
				{
					auto* v = JSWrapper::checkArg<const char*>(L, 3);
					InputBlob blob(v, stringLength(v) + 1);
					desc->set(cmp, -1, blob);
				}
				break;
				case PropertyDescriptorBase::COLOR:
				case PropertyDescriptorBase::VEC3:
				{
					auto v = JSWrapper::checkArg<Vec3>(L, 3);
					InputBlob blob(&v, sizeof(v));
					desc->set(cmp, -1, blob);
				}
				break;
				case PropertyDescriptorBase::VEC2:
				{
					auto v = JSWrapper::checkArg<Vec2>(L, 3);
					InputBlob blob(&v, sizeof(v));
					desc->set(cmp, -1, blob);
				}
				break;
				case PropertyDescriptorBase::INT2:
				{
					auto v = JSWrapper::checkArg<Int2>(L, 3);
					InputBlob blob(&v, sizeof(v));
					desc->set(cmp, -1, blob);
				}
				break;
				case PropertyDescriptorBase::ENTITY:
				{
					auto v = JSWrapper::checkArg<Entity>(L, 3);
					InputBlob blob(&v, sizeof(v));
					desc->set(cmp, -1, blob);
				}
				break;
				case PropertyDescriptorBase::ENUM:
				{
					auto v = JSWrapper::checkArg<int>(L, 3);
					InputBlob blob(&v, sizeof(v));
					desc->set(cmp, -1, blob);
				}
				break;
				default: JSL_argerror(L, 1, "Unsupported property type"); break;
			}
			return 0;
		}


		void registerProperties()
		{
			int cmps_count = PropertyRegister::getComponentTypesCount();
			JS_State* L = m_system.m_engine.getState();
			for (int i = 0; i < cmps_count; ++i)
			{
				const char* cmp_name = PropertyRegister::getComponentTypeID(i);
				JS_newtable(L);
				JS_pushvalue(L, -1);
				char tmp[50];
				convertPropertyToJSName(cmp_name, tmp, lengthOf(tmp));
				JS_setglobal(L, tmp);

				ComponentType cmp_type = PropertyRegister::getComponentType(cmp_name);
				auto& descs = PropertyRegister::getDescriptors(cmp_type);
				char setter[50];
				char getter[50];
				for (auto* desc : descs)
				{
					switch (desc->getType())
					{
						case PropertyDescriptorBase::ENTITY:
						case PropertyDescriptorBase::ENUM:
						case PropertyDescriptorBase::DECIMAL:
						case PropertyDescriptorBase::INTEGER:
						case PropertyDescriptorBase::BOOL:
						case PropertyDescriptorBase::VEC3:
						case PropertyDescriptorBase::VEC2:
						case PropertyDescriptorBase::INT2:
						case PropertyDescriptorBase::COLOR:
						case PropertyDescriptorBase::RESOURCE:
						case PropertyDescriptorBase::FILE:
						case PropertyDescriptorBase::STRING:
							convertPropertyToJSName(desc->getName(), tmp, lengthOf(tmp));
							copyString(setter, "set");
							copyString(getter, "get");
							catString(setter, tmp);
							catString(getter, tmp);
							JS_pushlightuserdata(L, desc);
							JS_pushinteger(L, cmp_type.index);
							JS_pushcclosure(L, &JS_setProperty, 2);
							JS_setfield(L, -2, setter);

							JS_pushlightuserdata(L, desc);
							JS_pushinteger(L, cmp_type.index);
							JS_pushcclosure(L, &JS_getProperty, 2);
							JS_setfield(L, -2, getter);
							break;
						default: break;
					}
				}
				JS_pop(L, 1);
			}
		}



		void cancelTimer(int timer_func)
		{
			for (int i = 0, c = m_timers.size(); i < c; ++i)
			{
				if (m_timers[i].func == timer_func)
				{
					m_timers.eraseFast(i);
					break;
				}
			}
		}


		static int setTimer(JS_State* L)
		{
			auto* scene = JSWrapper::checkArg<JSScriptSceneImpl*>(L, 1);
			float time = JSWrapper::checkArg<float>(L, 2);
			if (!JS_isfunction(L, 3)) JSWrapper::argError(L, 3, "function");
			TimerData& timer = scene->m_timers.emplace();
			timer.time = time;
			timer.state = L;
			JS_pushvalue(L, 3);
			timer.func = JSL_ref(L, JS_REGISTRYINDEX);
			JS_pop(L, 1);
			JSWrapper::push(L, timer.func);
			return 1;
		}


		void setScriptSource(ComponentHandle cmp, int scr_index, const char* path)
		{
			setScriptPath(cmp, scr_index, Path(path));
		}
		*/


		void getInstanceName(IScene& scene, char* out_str, int size)
		{
			copyString(out_str, size, "g_scene_");
			catString(out_str, size, scene.getPlugin().getName());
		}


		void registerAPI()
		{
			if (m_is_api_registered) return;
			m_is_api_registered = true;

			duk_context* ctx = m_system.m_global_context;
			registerGlobal(ctx, "Universe", "g_universe", &m_universe);

			registerJSObject(ctx, nullptr, "SceneBase", &ptrJSConstructor);

			Array<IScene*>& scenes = m_universe.getScenes();
			for (IScene* scene : scenes)
			{
				StaticString<50> type_name(scene->getPlugin().getName(), "_scene");
				char inst_name[50];
				getInstanceName(*scene, inst_name, lengthOf(inst_name));
				registerJSObject(ctx, "SceneBase", type_name, &ptrJSConstructor);
				registerGlobal(ctx, type_name, inst_name, scene);
			}

			//TODO
/*			
			registerProperties();
			registerPropertyAPI();
			JSWrapper::createSystemFunction(
				engine_state, "JSScript", "getEnvironment", &JSScriptSceneImpl::getEnvironment);
			
			#define REGISTER_FUNCTION(F) \
				do { \
					auto f = &JSWrapper::wrapMethod<JSScriptSceneImpl, \
						decltype(&JSScriptSceneImpl::F), \
						&JSScriptSceneImpl::F>; \
					JSWrapper::createSystemFunction(engine_state, "JSScript", #F, f); \
				} while(false)

			REGISTER_FUNCTION(addScript);
			REGISTER_FUNCTION(getScriptCount);
			REGISTER_FUNCTION(setScriptSource);
			REGISTER_FUNCTION(cancelTimer);

			#undef REGISTER_FUNCTION

			JSWrapper::createSystemFunction(engine_state, "JSScript", "setTimer", &JSScriptSceneImpl::setTimer);*/
		}

		
		int getEnvironment(ComponentHandle cmp, int scr_index) override
		{
			return m_scripts[{cmp.index}]->m_scripts[scr_index].m_environment;
		}


		const char* getPropertyName(u32 name_hash) const
		{
			int idx = m_property_names.find(name_hash);
			if(idx >= 0) return m_property_names.at(idx).c_str();
			return nullptr;
		}

		// TODO
		/*
		void applyResourceProperty(ScriptInstance& script, const char* name, Property& prop, const char* value)
		{
			bool is_env_valid = JS_rawgeti(script.m_state, JS_REGISTRYINDEX, script.m_environment) == JS_TTABLE;
			ASSERT(is_env_valid);
			JS_getfield(script.m_state, -1, name);
			int res_idx = JSWrapper::toType<int>(script.m_state, -1);
			m_system.m_engine.unloadJSResource(res_idx);
			JS_pop(script.m_state, 1);

			int new_res = m_system.m_engine.addJSResource(Path(value), prop.resource_type);
			JS_pushinteger(script.m_state, new_res);
			JS_setfield(script.m_state, -2, name);
			JS_pop(script.m_state, 1);
		}


		void applyProperty(ScriptInstance& script, Property& prop, const char* value)
		{
			if (!value) return;
			JS_State* state = script.m_state;
			const char* name = getPropertyName(prop.name_hash);
			if (!name) return;

			if (prop.type == Property::RESOURCE)
			{
				applyResourceProperty(script, name, prop, value);
				return;
			}

			StaticString<1024> tmp(name, " = ");
			if (prop.type == Property::STRING) tmp << "\"" << value << "\"";
			else tmp << value;

			bool errors = JSL_loadbuffer(state, tmp, stringLength(tmp), nullptr) != JS_OK;
			if (errors)
			{
				g_log_error.log("JS Script") << script.m_script->getPath() << ": " << JS_tostring(state, -1);
				JS_pop(state, 1);
				return;
			}

			bool is_env_valid = JS_rawgeti(script.m_state, JS_REGISTRYINDEX, script.m_environment) == JS_TTABLE;
			ASSERT(is_env_valid);
			JS_setupvalue(script.m_state, -2, 1);

			errors = errors || JS_pcall(state, 0, 0, 0) != JS_OK;

			if (errors)
			{
				g_log_error.log("JS Script") << script.m_script->getPath() << ": " << JS_tostring(state, -1);
				JS_pop(state, 1);
			}
		}
		*/

		void setPropertyValue(ComponentHandle cmp,
			int scr_index,
			const char* name,
			const char* value) override
		{
			// TODO
			/*auto* script_cmp = m_scripts[{cmp.index}];
			if (!script_cmp) return;
			Property& prop = getScriptProperty(cmp, scr_index, name);
			if (!script_cmp->m_scripts[scr_index].m_state)
			{
				prop.stored_value = value;
				return;
			}

			applyProperty(script_cmp->m_scripts[scr_index], prop, value);*/
		}


		const char* getPropertyName(ComponentHandle cmp, int scr_index, int index) const
		{
			auto& script = m_scripts[{cmp.index}]->m_scripts[scr_index];

			return getPropertyName(script.m_properties[index].name_hash);
		}


		int getPropertyCount(ComponentHandle cmp, int scr_index) const
		{
			auto& script = m_scripts[{cmp.index}]->m_scripts[scr_index];

			return script.m_properties.size();
		}


		static void* JSAllocator(void* ud, void* ptr, size_t osize, size_t nsize)
		{
			auto& allocator = *static_cast<IAllocator*>(ud);
			if (nsize == 0)
			{
				allocator.deallocate(ptr);
				return nullptr;
			}
			if (nsize > 0 && ptr == nullptr) return allocator.allocate(nsize);

			void* new_mem = allocator.allocate(nsize);
			copyMemory(new_mem, ptr, Math::minimum(osize, nsize));
			allocator.deallocate(ptr);
			return new_mem;
		}


		void destroyInstance(ScriptComponent& scr,  ScriptInstance& inst)
		{
			// TODO
			/*
			bool is_env_valid = JS_rawgeti(inst.m_state, JS_REGISTRYINDEX, inst.m_environment) == JS_TTABLE;
			ASSERT(is_env_valid);
			if (JS_getfield(inst.m_state, -1, "onDestroy") != JS_TFUNCTION)
			{
				JS_pop(inst.m_state, 2);
			}
			else
			{
				if (JS_pcall(inst.m_state, 0, 0, 0) != JS_OK)
				{
					g_log_error.log("JS Script") << JS_tostring(inst.m_state, -1);
					JS_pop(inst.m_state, 1);
				}
				JS_pop(inst.m_state, 1);
			}

			for (int i = 0; i < m_timers.size(); ++i)
			{
				if (m_timers[i].state == inst.m_state)
				{
					JSL_unref(m_timers[i].state, JS_REGISTRYINDEX, m_timers[i].func);
					m_timers.eraseFast(i);
					--i;
				}
			}

			for(int i = 0; i < m_updates.size(); ++i)
			{
				if(m_updates[i].state == inst.m_state)
				{
					m_updates.eraseFast(i);
					break;
				}
			}

			JSL_unref(inst.m_state, JS_REGISTRYINDEX, inst.m_thread_ref);
			JSL_unref(inst.m_state, JS_REGISTRYINDEX, inst.m_environment);
			inst.m_state = nullptr;*/
		}


		void setScriptPath(ScriptComponent& cmp, ScriptInstance& inst, const Path& path)
		{
			// TODO
			//registerAPI();

			if (inst.m_script)
			{
				if (inst.m_context) destroyInstance(cmp, inst);
				inst.m_properties.clear();
				auto& cb = inst.m_script->getObserverCb();
				cb.unbind<ScriptComponent, &ScriptComponent::onScriptLoaded>(&cmp);
				m_system.getScriptManager().unload(*inst.m_script);
			}
			inst.m_script = path.isValid() ? static_cast<JSScript*>(m_system.getScriptManager().load(path)) : nullptr;
			if (inst.m_script)
			{
				inst.m_script->onLoaded<ScriptComponent, &ScriptComponent::onScriptLoaded>(&cmp);
			}
		}


		void startScript(ScriptInstance& instance, bool is_restart)
		{
			// TODO
			/*
			if (is_restart)
			{
				for (int i = 0; i < m_updates.size(); ++i)
				{
					if (m_updates[i].state == instance.m_state)
					{
						m_updates.eraseFast(i);
						break;
					}
				}
			}

			if (JS_rawgeti(instance.m_state, JS_REGISTRYINDEX, instance.m_environment) != JS_TTABLE)
			{
				ASSERT(false);
				JS_pop(instance.m_state, 1);
				return;
			}
			if (JS_getfield(instance.m_state, -1, "update") == JS_TFUNCTION)
			{
				auto& update_data = m_updates.emplace();
				update_data.script = instance.m_script;
				update_data.state = instance.m_state;
				update_data.environment = instance.m_environment;
			}
			JS_pop(instance.m_state, 1);

			if (!is_restart)
			{
				if (JS_getfield(instance.m_state, -1, "init") != JS_TFUNCTION)
				{
					JS_pop(instance.m_state, 2);
					return;
				}

				if (JS_pcall(instance.m_state, 0, 0, 0) != JS_OK)
				{
					g_log_error.log("JS Script") << JS_tostring(instance.m_state, -1);
					JS_pop(instance.m_state, 1);
				}
			}
			JS_pop(instance.m_state, 1);*/
		}


		void startGame() override
		{
			m_is_game_running = true;
		}


		void stopGame() override
		{
			m_scripts_init_called = false;
			m_is_game_running = false;
			m_updates.clear();
			m_timers.clear();
		}


		ComponentHandle createComponent(ComponentType type, Entity entity) override
		{
			if (type != JS_SCRIPT_TYPE) return INVALID_COMPONENT;

			auto& allocator = m_system.m_allocator;
			ScriptComponent* script = LUMIX_NEW(allocator, ScriptComponent)(*this, allocator);
			ComponentHandle cmp = {entity.index};
			script->m_entity = entity;
			m_scripts.insert(entity, script);
			m_universe.addComponent(entity, type, this, cmp);

			return cmp;
		}


		void destroyComponent(ComponentHandle component, ComponentType type) override
		{
			if (type != JS_SCRIPT_TYPE) return;

			Entity entity = {component.index};
			auto* script = m_scripts[entity];
			for (auto& scr : script->m_scripts)
			{
				if (scr.m_context) destroyInstance(*script, scr);
				if (scr.m_script)
				{
					auto& cb = scr.m_script->getObserverCb();
					cb.unbind<ScriptComponent, &ScriptComponent::onScriptLoaded>(script);
					m_system.getScriptManager().unload(*scr.m_script);
				}
			}
			LUMIX_DELETE(m_system.m_allocator, script);
			m_scripts.erase(entity);
			m_universe.destroyComponent(entity, type, this, component);
		}


		void getPropertyValue(ComponentHandle cmp,
			int scr_index,
			const char* property_name,
			char* out,
			int max_size) override
		{
			ASSERT(max_size > 0);

			u32 hash = crc32(property_name);
			auto& inst = m_scripts[{cmp.index}]->m_scripts[scr_index];
			for (auto& prop : inst.m_properties)
			{
				if (prop.name_hash == hash)
				{
					if (inst.m_script->isReady())
						getProperty(prop, property_name, inst, out, max_size);
					else
						copyString(out, max_size, prop.stored_value.c_str());
					return;
				}
			}
			*out = '\0';
		}


		void getProperty(Property& prop, const char* prop_name, ScriptInstance& scr, char* out, int max_size)
		{
			// TODO
			/*
			if(max_size <= 0) return;
			if (!scr.m_state)
			{
				copyString(out, max_size, prop.stored_value.c_str());
				return;
			}

			*out = '\0';
			JS_rawgeti(scr.m_state, JS_REGISTRYINDEX, scr.m_environment);
			if (JS_getfield(scr.m_state, -1, prop_name) == JS_TNIL)
			{
				copyString(out, max_size, prop.stored_value.c_str());
				JS_pop(scr.m_state, 2);
				return;
			}
			switch (prop.type)
			{
				case Property::BOOLEAN:
				{
					bool b = JS_toboolean(scr.m_state, -1) != 0;
					copyString(out, max_size, b ? "true" : "false");
				}
				break;
				case Property::FLOAT:
				{
					float val = (float)JS_tonumber(scr.m_state, -1);
					toCString(val, out, max_size, 8);
				}
				break;
				case Property::ENTITY:
				{
					Entity val = { (int)JS_tointeger(scr.m_state, -1) };
					toCString(val.index, out, max_size);
				}
				break;
				case Property::STRING:
				{
					copyString(out, max_size, JS_tostring(scr.m_state, -1));
				}
				break;
				case Property::RESOURCE:
				{
					int res_idx = JSWrapper::toType<int>(scr.m_state, -1);
					Resource* res = m_system.m_engine.getJSResource(res_idx);
					copyString(out, max_size, res ? res->getPath().c_str() : "");
				}
				break;
				default: ASSERT(false); break;
			}
			JS_pop(scr.m_state, 2);
			*/
		}


		void serializeJSScript(ISerializer& serializer, ComponentHandle cmp)
		{
			// TODO
			/*
			ScriptComponent* script = m_scripts[{cmp.index}];
			serializer.write("count", script->m_scripts.size());
			for (ScriptInstance& inst : script->m_scripts)
			{
				serializer.write("source", inst.m_script ? inst.m_script->getPath().c_str() : "");
				serializer.write("prop_count", inst.m_properties.size());
				for (Property& prop : inst.m_properties)
				{
					const char* name = getPropertyName(prop.name_hash);
					serializer.write("prop_name", name ? name : "");
					int idx = m_property_names.find(prop.name_hash);
					if (idx >= 0)
					{
						const char* name = m_property_names.at(idx).c_str();
						serializer.write("prop_type", (int)prop.type);
						if (prop.type == Property::ENTITY)
						{
							JS_rawgeti(inst.m_state, JS_REGISTRYINDEX, inst.m_environment);
							if (JS_getfield(inst.m_state, -1, name) == JS_TNIL)
							{
								serializer.write("prop_value", prop.stored_value.c_str());
							}
							else
							{
								Entity val = {(int)JS_tointeger(inst.m_state, -1)};
								EntityGUID guid = serializer.getGUID(val);
								char tmp[128];
								toCString(guid.value, tmp, lengthOf(tmp));
								serializer.write("prop_value", tmp);
							}
						}
						else
						{
							char tmp[1024];
							getProperty(prop, name, inst, tmp, lengthOf(tmp));
							serializer.write("prop_value", tmp);
						}
					}
					else
					{
						serializer.write("prop_type", (int)Property::ANY);
						serializer.write("prop_value", "");
					}
				}
			}
			*/
		}


		void deserializeJSScript(IDeserializer& serializer, Entity entity, int scene_version)
		{
			auto& allocator = m_system.m_allocator;
			ScriptComponent* script = LUMIX_NEW(allocator, ScriptComponent)(*this, allocator);
			ComponentHandle cmp = {entity.index};
			script->m_entity = entity;
			m_scripts.insert(entity, script);
			
			int count;
			serializer.read(&count);
			script->m_scripts.reserve(count);
			auto* manager = m_system.m_engine.getResourceManager().get(JS_SCRIPT_RESOURCE_TYPE);
			for (int i = 0; i < count; ++i)
			{
				ScriptInstance& inst = script->m_scripts.emplace(allocator);
				char tmp[MAX_PATH_LENGTH];
				serializer.read(tmp, lengthOf(tmp));
				setScriptPath(cmp, i, Path(tmp));

				int prop_count;
				serializer.read(&prop_count);
				for (int j = 0; j < prop_count; ++j)
				{
					Property& prop = inst.m_properties.emplace(allocator);
					prop.type = Property::ANY;
					char tmp[1024];
					serializer.read(tmp, lengthOf(tmp));
					prop.name_hash = crc32(tmp);
					if (m_property_names.find(prop.name_hash) < 0)
					{
						m_property_names.emplace(prop.name_hash, tmp, allocator);
					}
					tmp[0] = 0;
					if (scene_version > (int)JSSceneVersion::PROPERTY_TYPE) serializer.read((int*)&prop.type);
					serializer.read(tmp, lengthOf(tmp));
					
					if (prop.type == Property::ENTITY)
					{
						u64 guid;
						fromCString(tmp, lengthOf(tmp), &guid);
						Entity entity = serializer.getEntity({guid});
						toCString(entity.index, tmp, lengthOf(tmp));
					}
					prop.stored_value = tmp;
				}
			}

			m_universe.addComponent(entity, JS_SCRIPT_TYPE, this, cmp);
		}


		void serialize(OutputBlob& serializer) override
		{
			serializer.write(m_scripts.size());
			for (auto iter = m_scripts.begin(), end = m_scripts.end(); iter != end; ++iter)
			{
				ScriptComponent* script_cmp = iter.value();
				serializer.write(script_cmp->m_entity);
				serializer.write(script_cmp->m_scripts.size());
				for (auto& scr : script_cmp->m_scripts)
				{
					serializer.writeString(scr.m_script ? scr.m_script->getPath().c_str() : "");
					serializer.write(scr.m_properties.size());
					for (Property& prop : scr.m_properties)
					{
						serializer.write(prop.name_hash);
						int idx = m_property_names.find(prop.name_hash);
						if (idx >= 0)
						{
							const char* name = m_property_names.at(idx).c_str();
							char tmp[1024];
							getProperty(prop, name, scr, tmp, lengthOf(tmp));
							serializer.writeString(tmp);
						}
						else
						{
							serializer.writeString("");
						}
					}
				}
			}
		}


		void deserialize(InputBlob& serializer) override
		{
			int len = serializer.read<int>();
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
					scr.m_context = nullptr;
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
				ComponentHandle cmp = {script->m_entity.index};
				m_universe.addComponent(script->m_entity, JS_SCRIPT_TYPE, this, cmp);
			}
		}


		IPlugin& getPlugin() const override { return m_system; }


		void initScripts()
		{
			ASSERT(!m_scripts_init_called && m_is_game_running);
			// copy m_scripts to tmp, because scripts can create other scripts -> m_scripts is not const
			Array<ScriptComponent*> tmp(m_system.m_allocator);
			tmp.reserve(m_scripts.size());
			for (auto* scr : m_scripts) tmp.push(scr);

			for (auto* scr : tmp)
			{
				for (int j = 0; j < scr->m_scripts.size(); ++j)
				{
					auto& instance = scr->m_scripts[j];
					if (!instance.m_script) continue;
					if (!instance.m_script->isReady()) continue;

					startScript(instance, false);
				}
			}
			m_scripts_init_called = true;
		}


		void updateTimers(float time_delta)
		{
			// TODO
			/*
			int timers_to_remove[1024];
			int timers_to_remove_count = 0;
			for (int i = 0, c = m_timers.size(); i < c; ++i)
			{
				auto& timer = m_timers[i];
				timer.time -= time_delta;
				if (timer.time < 0)
				{
					if (JS_rawgeti(timer.state, JS_REGISTRYINDEX, timer.func) != JS_TFUNCTION)
					{
						ASSERT(false);
					}

					if (JS_pcall(timer.state, 0, 0, 0) != JS_OK)
					{
						g_log_error.log("JS Script") << JS_tostring(timer.state, -1);
						JS_pop(timer.state, 1);
					}
					timers_to_remove[timers_to_remove_count] = i;
					++timers_to_remove_count;
					if (timers_to_remove_count >= lengthOf(timers_to_remove))
					{
						g_log_error.log("JS Script") << "Too many JS timers in one frame, some are not executed";
						break;
					}
				}
			}
			for (int i = timers_to_remove_count - 1; i >= 0; --i)
			{
				auto& timer = m_timers[timers_to_remove[i]];
				JSL_unref(timer.state, JS_REGISTRYINDEX, timer.func);
				m_timers.eraseFast(timers_to_remove[i]);
			}*/
		}


		void update(float time_delta, bool paused) override
		{
			// TODO
			/*
			PROFILE_FUNCTION();

			if (m_is_game_running && !m_scripts_init_called) initScripts();

			if (paused) return;

			updateTimers(time_delta);

			for (int i = 0; i < m_updates.size(); ++i)
			{
				UpdateData update_item = m_updates[i];
				if (JS_rawgeti(update_item.state, JS_REGISTRYINDEX, update_item.environment) != JS_TTABLE)
				{
					ASSERT(false);
				}
				if (JS_getfield(update_item.state, -1, "update") != JS_TFUNCTION)
				{
					JS_pop(update_item.state, 2);
					continue;
				}

				JS_pushnumber(update_item.state, time_delta);
				if (JS_pcall(update_item.state, 1, 0, 0) != JS_OK)
				{
					g_log_error.log("JS Script") << JS_tostring(update_item.state, -1);
					JS_pop(update_item.state, 1);
				}
				JS_pop(update_item.state, 1);
			}
			*/
		}


		ComponentHandle getComponent(Entity entity, ComponentType type) override
		{
			if (m_scripts.find(entity) == m_scripts.end()) return INVALID_COMPONENT;
			return {entity.index};
		}


		Property& getScriptProperty(ComponentHandle cmp, int scr_index, const char* name)
		{
			u32 name_hash = crc32(name);
			ScriptComponent* script_cmp = m_scripts[{cmp.index}];
			for (auto& prop : script_cmp->m_scripts[scr_index].m_properties)
			{
				if (prop.name_hash == name_hash)
				{
					return prop;
				}
			}

			script_cmp->m_scripts[scr_index].m_properties.emplace(m_system.m_allocator);
			auto& prop = script_cmp->m_scripts[scr_index].m_properties.back();
			prop.name_hash = name_hash;
			return prop;
		}


		Path getScriptPath(ComponentHandle cmp, int scr_index) override
		{
			auto& tmp = m_scripts[{cmp.index}]->m_scripts[scr_index];
			return tmp.m_script ? tmp.m_script->getPath() : Path("");
		}


		void setScriptPath(ComponentHandle cmp, int scr_index, const Path& path) override
		{
			auto* script_cmp = m_scripts[{cmp.index}];
			if (script_cmp->m_scripts.size() <= scr_index) return;
			setScriptPath(*script_cmp, script_cmp->m_scripts[scr_index], path);
		}


		int getScriptCount(ComponentHandle cmp) override
		{
			return m_scripts[{cmp.index}]->m_scripts.size();
		}


		void insertScript(ComponentHandle cmp, int idx) override
		{
			m_scripts[{cmp.index}]->m_scripts.emplaceAt(idx, m_system.m_allocator);
		}


		int addScript(ComponentHandle cmp) override
		{
			ScriptComponent* script_cmp = m_scripts[{cmp.index}];
			script_cmp->m_scripts.emplace(m_system.m_allocator);
			return script_cmp->m_scripts.size() - 1;
		}


		void moveScript(ComponentHandle cmp, int scr_index, bool up) override
		{
			auto* script_cmp = m_scripts[{cmp.index}];
			if (!up && scr_index > script_cmp->m_scripts.size() - 2) return;
			if (up && scr_index == 0) return;
			int other = up ? scr_index - 1 : scr_index + 1;
			ScriptInstance tmp = script_cmp->m_scripts[scr_index];
			script_cmp->m_scripts[scr_index] = script_cmp->m_scripts[other];
			script_cmp->m_scripts[other] = tmp;
		}


		void removeScript(ComponentHandle cmp, int scr_index) override
		{
			setScriptPath(cmp, scr_index, Path());
			m_scripts[{cmp.index}]->m_scripts.eraseFast(scr_index);
		}


		void serializeScript(ComponentHandle cmp, int scr_index, OutputBlob& blob) override
		{
			auto& scr = m_scripts[{cmp.index}]->m_scripts[scr_index];
			blob.writeString(scr.m_script ? scr.m_script->getPath().c_str() : "");
			blob.write(scr.m_properties.size());
			for (auto prop : scr.m_properties)
			{
				blob.write(prop.name_hash);
				char tmp[1024];
				const char* property_name = getPropertyName(prop.name_hash);
				if (!property_name)
				{
					blob.writeString(prop.stored_value.c_str());
				}
				else
				{
					getProperty(prop, property_name, scr, tmp, lengthOf(tmp));
					blob.writeString(tmp);
				}
			}
		}


		void deserializeScript(ComponentHandle cmp, int scr_index, InputBlob& blob) override
		{
			auto& scr = m_scripts[{cmp.index}]->m_scripts[scr_index];
			int count;
			char path[MAX_PATH_LENGTH];
			blob.readString(path, lengthOf(path));
			blob.read(count);
			scr.m_environment = -1;
			scr.m_properties.clear();
			char buf[256];
			for (int i = 0; i < count; ++i)
			{
				auto& prop = scr.m_properties.emplace(m_system.m_allocator);
				prop.type = Property::ANY;
				blob.read(prop.name_hash);
				blob.readString(buf, lengthOf(buf));
				prop.stored_value = buf;
			}
			setScriptPath(cmp, scr_index, Path(path));
		}


		JSScriptSystemImpl& m_system;
		HashMap<Entity, ScriptComponent*> m_scripts;
		AssociativeArray<u32, string> m_property_names;
		Universe& m_universe;
		Array<UpdateData> m_updates;
		Array<TimerData> m_timers;
		FunctionCall m_function_call;
		ScriptInstance* m_current_script_instance;
		bool m_scripts_init_called = false;
		bool m_is_api_registered = false;
		bool m_is_game_running = false;
	};


	JSScriptSystemImpl::JSScriptSystemImpl(Engine& engine)
		: m_engine(engine)
		, m_allocator(engine.getAllocator())
		, m_script_manager(m_allocator)
	{
		m_script_manager.create(JS_SCRIPT_RESOURCE_TYPE, engine.getResourceManager());

		auto& allocator = engine.getAllocator();
		PropertyRegister::add("js_script",
			LUMIX_NEW(allocator, BlobPropertyDescriptor<JSScriptScene>)(
				"data", &JSScriptScene::getScriptData, &JSScriptScene::setScriptData));

		m_global_context = duk_create_heap_default();
		registerGlobalAPI();
	}


	static void logError(const char* msg) { g_log_error.log("JS Script") << msg; }
	static void logWarning(const char* msg) { g_log_warning.log("JS Script") << msg; }
	static void logInfo(const char* msg) { g_log_info.log("JS Script") << msg; }


	static void convertPropertyToJSName(const char* src, char* out, int max_size)
	{
		ASSERT(max_size > 0);
		bool to_upper = true;
		char* dest = out;
		while (*src && dest - out < max_size - 1)
		{
			if (isLetter(*src))
			{
				*dest = to_upper && !isUpperCase(*src) ? *src - 'a' + 'A' : *src;
				to_upper = false;
				++dest;
			}
			else if (isNumeric(*src))
			{
				*dest = *src;
				++dest;
			}
			else
			{
				to_upper = true;
			}
			++src;
		}
		*dest = 0;
	}


	static int JS_getProperty(duk_context* ctx)
	{
		duk_push_this(ctx);
		if (duk_is_null_or_undefined(ctx, -1))
		{
			duk_eval_error(ctx, "this is null or undefined");
		}
		duk_get_prop_string(ctx, -1, "c_scene");
		IScene* scene = JSWrapper::toType<IScene*>(ctx, -1);
		if(!scene) duk_eval_error(ctx, "getting property on invalid object");

		duk_get_prop_string(ctx, -1, "c_cmphandle");
		ComponentHandle cmp_handle = JSWrapper::toType<ComponentHandle>(ctx, -1);
		duk_get_prop_string(ctx, -1, "c_cmptype");
		ComponentType cmp_type = { JSWrapper::toType<int>(ctx, -1) };
		duk_pop(ctx);

		duk_push_current_function(ctx);
		duk_get_prop_string(ctx, -1, "c_desc");
		PropertyDescriptorBase* desc = JSWrapper::toType<PropertyDescriptorBase*>(ctx, -1);
		duk_pop(ctx);

		ComponentUID cmp;
		cmp.scene = scene;
		cmp.handle = cmp_handle;
		cmp.type = cmp_type;
		cmp.entity = INVALID_ENTITY;
		switch (desc->getType())
		{
			case PropertyDescriptorBase::DECIMAL:
			{
				float v;
				OutputBlob blob(&v, sizeof(v));
				desc->get(cmp, -1, blob);
				JSWrapper::push(ctx, v);
			}
			break;
			case PropertyDescriptorBase::BOOL:
			{
				bool v;
				OutputBlob blob(&v, sizeof(v));
				desc->get(cmp, -1, blob);
				JSWrapper::push(ctx, v);
			}
			break;
			case PropertyDescriptorBase::INTEGER:
			{
				int v;
				OutputBlob blob(&v, sizeof(v));
				desc->get(cmp, -1, blob);
				JSWrapper::push(ctx, v);
			}
			break;
			case PropertyDescriptorBase::COLOR:
			case PropertyDescriptorBase::VEC3:
			{
				Vec3 v;
				OutputBlob blob(&v, sizeof(v));
				desc->get(cmp, -1, blob);
				JSWrapper::push(ctx, v);
			}
			break;
			case PropertyDescriptorBase::RESOURCE:
			case PropertyDescriptorBase::FILE:
			case PropertyDescriptorBase::STRING:
			{
				char buf[1024];
				OutputBlob blob(buf, sizeof(buf));
				desc->get(cmp, -1, blob);
				JSWrapper::push(ctx, buf);
			}
			break;
			case PropertyDescriptorBase::ENTITY:
			{
				Entity v;
				OutputBlob blob(&v, sizeof(v));
				desc->get(cmp, -1, blob);
				JSWrapper::push(ctx, v);
			}
			break;
			case PropertyDescriptorBase::ENUM:
			{
				int v;
				OutputBlob blob(&v, sizeof(v));
				desc->get(cmp, -1, blob);
				JSWrapper::push(ctx, v);
			}
			break;
			case PropertyDescriptorBase::VEC2:
			{
				Vec2 v;
				OutputBlob blob(&v, sizeof(v));
				desc->get(cmp, -1, blob);
				JSWrapper::push(ctx, v);
			}
			break;
			case PropertyDescriptorBase::INT2:
			{
				Int2 v;
				OutputBlob blob(&v, sizeof(v));
				desc->get(cmp, -1, blob);
				JSWrapper::push(ctx, v);
			}
			break;
			default: duk_error(ctx, DUK_ERR_TYPE_ERROR, "Unsupported property type"); break;
		}
		return 1;
	}

	
	static int JS_setProperty(duk_context* ctx)
	{
		duk_push_this(ctx);
		if (duk_is_null_or_undefined(ctx, -1))
		{
			duk_eval_error(ctx, "this is null or undefined");
		}
		duk_get_prop_string(ctx, -1, "c_scene");
		IScene* scene = JSWrapper::toType<IScene*>(ctx, -1);
		if (!scene) duk_eval_error(ctx, "getting property on invalid object");

		duk_get_prop_string(ctx, -1, "c_cmphandle");
		ComponentHandle cmp_handle = JSWrapper::toType<ComponentHandle>(ctx, -1);
		duk_get_prop_string(ctx, -1, "c_cmptype");
		ComponentType cmp_type = { JSWrapper::toType<int>(ctx, -1) };
		duk_pop(ctx);

		duk_push_current_function(ctx);
		duk_get_prop_string(ctx, -1, "c_desc");
		PropertyDescriptorBase* desc = JSWrapper::toType<PropertyDescriptorBase*>(ctx, -1);
		duk_pop(ctx);

		ComponentUID cmp;
		cmp.scene = scene;
		cmp.handle = cmp_handle;
		cmp.type = cmp_type;
		cmp.entity = INVALID_ENTITY;
		switch (desc->getType())
		{
			case PropertyDescriptorBase::DECIMAL:
			{
				auto v = JSWrapper::checkArg<float>(ctx, 0);
				InputBlob blob(&v, sizeof(v));
				desc->set(cmp, -1, blob);
			}
			break;
			case PropertyDescriptorBase::INTEGER:
			{
				auto v = JSWrapper::checkArg<int>(ctx, 0);
				InputBlob blob(&v, sizeof(v));
				desc->set(cmp, -1, blob);
			}
			break;
			case PropertyDescriptorBase::BOOL:
			{
				auto v = JSWrapper::checkArg<bool>(ctx, 0);
				InputBlob blob(&v, sizeof(v));
				desc->set(cmp, -1, blob);
			}
			break;
			case PropertyDescriptorBase::RESOURCE:
			case PropertyDescriptorBase::FILE:
			case PropertyDescriptorBase::STRING:
			{
				auto* v = JSWrapper::checkArg<const char*>(ctx, 0);
				InputBlob blob(v, stringLength(v) + 1);
				desc->set(cmp, -1, blob);
			}
			break;
			case PropertyDescriptorBase::COLOR:
			case PropertyDescriptorBase::VEC3:
			{
				auto v = JSWrapper::checkArg<Vec3>(ctx, 0);
				InputBlob blob(&v, sizeof(v));
				desc->set(cmp, -1, blob);
			}
			break;
			case PropertyDescriptorBase::VEC2:
			{
				auto v = JSWrapper::checkArg<Vec2>(ctx, 0);
				InputBlob blob(&v, sizeof(v));
				desc->set(cmp, -1, blob);
			}
			break;
			case PropertyDescriptorBase::INT2:
			{
				auto v = JSWrapper::checkArg<Int2>(ctx, 0);
				InputBlob blob(&v, sizeof(v));
				desc->set(cmp, -1, blob);
			}
			break;
			// TODO
			/*case PropertyDescriptorBase::ENTITY:
			{
				auto v = JSWrapper::checkArg<Entity>(ctx, 0);
				InputBlob blob(&v, sizeof(v));
				desc->set(cmp, -1, blob);
			}
			break;*/
			case PropertyDescriptorBase::ENUM:
			{
				auto v = JSWrapper::checkArg<int>(ctx, 0);
				InputBlob blob(&v, sizeof(v));
				desc->set(cmp, -1, blob);
			}
			break;
			default: duk_error(ctx, DUK_ERR_TYPE_ERROR, "Unsupported property type"); break;
		}
		return 0;
	}


	#define REGISTER_JS_METHOD(O, F) \
		do { \
			auto f = &JSWrapper::wrapMethod<O, decltype(&O::F), &O::F>; \
			registerMethod(m_global_context, #O, #F, f); \
		} while(false)


	static void registerComponent(duk_context* ctx, const char* cmp_type_name)
	{
		auto cmp_type = PropertyRegister::getComponentType(cmp_type_name);
		registerJSComponent(ctx, cmp_type, cmp_type_name, &componentJSConstructor);
		auto& descs = PropertyRegister::getDescriptors(cmp_type);

		char tmp[50];
		char setter[50];
		char getter[50];
		for (auto* desc : descs)
		{
			switch (desc->getType())
			{
				case PropertyDescriptorBase::ENUM:
				// TODO
				//case PropertyDescriptorBase::ENTITY:
				case PropertyDescriptorBase::COLOR:
				case PropertyDescriptorBase::VEC3:
				case PropertyDescriptorBase::DECIMAL:
				case PropertyDescriptorBase::INTEGER:
				case PropertyDescriptorBase::BOOL:
				case PropertyDescriptorBase::RESOURCE:
				case PropertyDescriptorBase::FILE:
				case PropertyDescriptorBase::STRING:
				case PropertyDescriptorBase::VEC2:
				case PropertyDescriptorBase::INT2:
					convertPropertyToJSName(desc->getName(), tmp, lengthOf(tmp));
					copyString(setter, "set");
					copyString(getter, "get");
					catString(setter, tmp);
					catString(getter, tmp);

					duk_get_global_string(ctx, cmp_type_name);
					if (duk_get_prop_string(ctx, -1, "prototype") != 1)
					{
						ASSERT(false);
					}

					duk_push_string(ctx, tmp);

					duk_push_c_function(ctx, JS_getProperty, 0);
					JSWrapper::push(ctx, desc);
					duk_put_prop_string(ctx, -2, "c_desc");

					duk_push_c_function(ctx, JS_setProperty, 1);
					JSWrapper::push(ctx, desc);
					duk_put_prop_string(ctx, -2, "c_desc");

					duk_def_prop(ctx, -4, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER | DUK_DEFPROP_ENUMERABLE);

					duk_pop_2(ctx);
					break;
				default: break;
			}
		}
	}


	void JSScriptSystemImpl::registerGlobalAPI()
	{
		#define REGISTER_JS_FUNCTION(F) \
			do { \
				duk_push_c_function(m_global_context, &JSWrapper::wrap<decltype(F), &F>, DUK_VARARGS); \
				duk_put_global_string(m_global_context, #F); \
			} while(false)

		REGISTER_JS_FUNCTION(logError);
		REGISTER_JS_FUNCTION(logWarning);
		REGISTER_JS_FUNCTION(logInfo);

		registerJSObject(m_global_context, nullptr, "Engine", &ptrJSConstructor);
		registerGlobal(m_global_context, "Engine", "g_engine", &m_engine);

		REGISTER_JS_METHOD(Engine, pause);
		REGISTER_JS_METHOD(Engine, nextFrame);
		REGISTER_JS_METHOD(Engine, startGame);
		REGISTER_JS_METHOD(Engine, stopGame);
		REGISTER_JS_METHOD(Engine, getFPS);
		REGISTER_JS_METHOD(Engine, getTime);
		REGISTER_JS_METHOD(Engine, getLastTimeDelta);

		registerJSObject(m_global_context, nullptr, "Universe", &ptrJSConstructor);
//		REGISTER_JS_METHOD(Universe, createEntity);
//		REGISTER_JS_METHOD(Universe, destroyEntity);
//		REGISTER_JS_METHOD(Universe, setEntityName);
//		REGISTER_JS_METHOD(Universe, getEntityName);
		//REGISTER_JS_METHOD(Universe, getEntityByName);
		//REGISTER_JS_METHOD(Universe, setScale);

		registerJSObject(m_global_context, nullptr, "Entity", &entityJSConstructor);

		#undef REGISTER_JS_FUNCTION

		int count = PropertyRegister::getComponentTypesCount();
		for (int i = 0; i < count; ++i)
		{
			const char* cmp_type_id = PropertyRegister::getComponentTypeID(i);
			registerComponent(m_global_context, cmp_type_id);
		}
	}

	#undef REGISTER_JS_METHOD

	JSScriptSystemImpl::~JSScriptSystemImpl()
	{
		duk_destroy_heap(m_global_context);
		m_script_manager.destroy();
	}


	void JSScriptSystemImpl::createScenes(Universe& ctx)
	{
		auto* scene = LUMIX_NEW(m_allocator, JSScriptSceneImpl)(*this, ctx);
		ctx.addScene(scene);
	}


	void JSScriptSystemImpl::destroyScene(IScene* scene)
	{
		LUMIX_DELETE(m_allocator, scene);
	}


	LUMIX_PLUGIN_ENTRY(lumixengine_js)
	{
		return LUMIX_NEW(engine.getAllocator(), JSScriptSystemImpl)(engine);
	}
}
