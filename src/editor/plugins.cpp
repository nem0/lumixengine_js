#define LUMIX_NO_CUSTOM_CRT
#include "../js_script_manager.h"
#include "../js_script_system.h"
#include "editor/asset_browser.h"
#include "editor/asset_compiler.h"
#include "editor/property_grid.h"
#include "editor/studio_app.h"
#include "editor/utils.h"
#include "editor/world_editor.h"
#include "engine/allocators.h"
#include "engine/array.h"
#include "engine/engine.h"
#include "engine/file_system.h"
#include "engine/log.h"
#include "engine/os.h"
#include "engine/path.h"
#include "engine/plugin.h"
#include "engine/reflection.h"
#include "engine/resource_manager.h"
#include "engine/universe.h"
#include "imgui/imgui.h"
#include <cstdlib>


using namespace Lumix;


static const ComponentType JS_SCRIPT_TYPE = reflection::getComponentType("js_script");


namespace {


struct PropertyGridPlugin final : public PropertyGrid::IPlugin {
	struct AddScriptCommand final : public IEditorCommand {
		AddScriptCommand() {}

		explicit AddScriptCommand(WorldEditor& editor) { scene = static_cast<JSScriptScene*>(editor.getUniverse()->getScene("js_script")); }

		bool execute() override {
			scr_index = scene->addScript(cmp);
			return true;
		}

		void undo() override { scene->removeScript(cmp, scr_index); }
		const char* getType() override { return "add_js_script"; }
		bool merge(IEditorCommand& command) override { return false; }

		JSScriptScene* scene;
		EntityRef cmp;
		int scr_index;
	};


	struct MoveScriptCommand final : public IEditorCommand {
		explicit MoveScriptCommand(WorldEditor& editor)
			: blob(editor.getAllocator())
			, scr_index(-1)
			, cmp(INVALID_ENTITY)
			, up(true) {
			scene = static_cast<JSScriptScene*>(editor.getUniverse()->getScene("js_script"));
		}

		explicit MoveScriptCommand(IAllocator& allocator)
			: blob(allocator)
			, scene(nullptr)
			, scr_index(-1)
			, cmp(INVALID_ENTITY)
			, up(true) {}

		bool execute() override {
			scene->moveScript((EntityRef)cmp, scr_index, up);
			return true;
		}

		void undo() override { scene->moveScript((EntityRef)cmp, up ? scr_index - 1 : scr_index + 1, !up); }
		const char* getType() override { return "move_js_script"; }
		bool merge(IEditorCommand& command) override { return false; }

		OutputMemoryStream blob;
		JSScriptScene* scene;
		EntityPtr cmp;
		int scr_index;
		bool up;
	};


	struct RemoveScriptCommand final : public IEditorCommand {
		explicit RemoveScriptCommand(WorldEditor& editor)
			: blob(editor.getAllocator())
			, scr_index(-1)
			, cmp(INVALID_ENTITY) {
			scene = static_cast<JSScriptScene*>(editor.getUniverse()->getScene("js_script"));
		}

		explicit RemoveScriptCommand(IAllocator& allocator)
			: blob(allocator)
			, scene(nullptr)
			, scr_index(-1)
			, cmp(INVALID_ENTITY) {}

		bool execute() override {
			// TODO
			ASSERT(false);
			// scene->serializeScript(cmp, scr_index, blob);
			scene->removeScript(cmp, scr_index);
			return true;
		}

		void undo() override {
			scene->insertScript(cmp, scr_index);
			InputMemoryStream input(blob);
			// TODO
			ASSERT(false);
			// scene->deserializeScript(cmp, scr_index, input);
		}

		const char* getType() override { return "remove_js_script"; }
		bool merge(IEditorCommand& command) override { return false; }

		OutputMemoryStream blob;
		JSScriptScene* scene;
		EntityRef cmp;
		int scr_index;
	};


	struct SetPropertyCommand final : public IEditorCommand {
		explicit SetPropertyCommand(WorldEditor& _editor)
			: property_name(_editor.getAllocator())
			, value(_editor.getAllocator())
			, old_value(_editor.getAllocator())
			, editor(_editor) {}


		SetPropertyCommand(WorldEditor& _editor, EntityRef cmp, int scr_index, const char* property_name, const char* val, IAllocator& allocator)
			: property_name(property_name, allocator)
			, value(val, allocator)
			, old_value(allocator)
			, component(cmp)
			, script_index(scr_index)
			, editor(_editor) {
			auto* scene = static_cast<JSScriptScene*>(editor.getUniverse()->getScene("js_script"));
			if (property_name[0] == '-') {
				old_value = scene->getScriptPath(component, script_index).c_str();
			} else {
				char tmp[1024];
				tmp[0] = '\0';
				scene->getPropertyValue(cmp, scr_index, property_name, tmp, lengthOf(tmp));
				old_value = tmp;
				return;
			}
		}


		bool execute() override {
			auto* scene = static_cast<JSScriptScene*>(editor.getUniverse()->getScene("js_script"));
			if (property_name.length() > 0 && property_name[0] == '-') {
				scene->setScriptPath(component, script_index, Path(value.c_str()));
			} else {
				scene->setPropertyValue(component, script_index, property_name.c_str(), value.c_str());
			}
			return true;
		}


		void undo() override {
			auto* scene = static_cast<JSScriptScene*>(editor.getUniverse()->getScene("js_script"));
			if (property_name.length() > 0 && property_name[0] == '-') {
				scene->setScriptPath(component, script_index, Path(old_value.c_str()));
			} else {
				scene->setPropertyValue(component, script_index, property_name.c_str(), old_value.c_str());
			}
		}


		const char* getType() override { return "set_js_script_property"; }


		bool merge(IEditorCommand& command) override {
			auto& cmd = static_cast<SetPropertyCommand&>(command);
			if (cmd.script_index == script_index && cmd.property_name == property_name) {
				// cmd.scene = scene;
				cmd.value = value;
				return true;
			}
			return false;
		}


		WorldEditor& editor;
		String property_name;
		String value;
		String old_value;
		EntityRef component;
		int script_index;
	};


	explicit PropertyGridPlugin(StudioApp& app)
		: m_app(app) {}


	void onGUI(PropertyGrid& grid, ComponentUID cmp, WorldEditor& editor) override {
		if (cmp.type != JS_SCRIPT_TYPE) return;

		const EntityRef entity = (EntityRef)cmp.entity;
		auto* scene = static_cast<JSScriptScene*>(cmp.scene);
		IAllocator& allocator = editor.getAllocator();

		if (ImGui::Button("Add script")) {
			UniquePtr<AddScriptCommand> cmd = UniquePtr<AddScriptCommand>::create(allocator);
			cmd->scene = scene;
			cmd->cmp = entity;
			editor.executeCommand(cmd.move());
		}

		for (int j = 0; j < scene->getScriptCount(entity); ++j) {
			char buf[LUMIX_MAX_PATH];
			copyString(buf, scene->getScriptPath(entity, j).c_str());
			StaticString<LUMIX_MAX_PATH + 20> header;
			copyString(Span(header.data), Path::getBasename(buf));
			if (header.empty()) header << j;
			header << "###" << j;
			if (ImGui::CollapsingHeader(header)) {
				ImGui::PushID(j);
				if (ImGui::Button("Remove script")) {
					UniquePtr<RemoveScriptCommand> cmd = UniquePtr<RemoveScriptCommand>::create(allocator, allocator);
					cmd->cmp = entity;
					cmd->scr_index = j;
					cmd->scene = scene;
					editor.executeCommand(cmd.move());
					ImGui::PopID();
					break;
				}
				ImGui::SameLine();
				if (ImGui::Button("Up")) {
					UniquePtr<MoveScriptCommand> cmd = UniquePtr<MoveScriptCommand>::create(allocator, allocator);
					cmd->cmp = entity;
					cmd->scr_index = j;
					cmd->scene = scene;
					cmd->up = true;
					editor.executeCommand(cmd.move());
					ImGui::PopID();
					break;
				}
				ImGui::SameLine();
				if (ImGui::Button("Down")) {
					UniquePtr<MoveScriptCommand> cmd = UniquePtr<MoveScriptCommand>::create(allocator, allocator);
					cmd->cmp = entity;
					cmd->scr_index = j;
					cmd->scene = scene;
					cmd->up = false;
					editor.executeCommand(cmd.move());
					ImGui::PopID();
					break;
				}

				ImGuiEx::Label("Source");
				if (m_app.getAssetBrowser().resourceInput("src", Span(buf), JSScript::TYPE)) {
					UniquePtr<SetPropertyCommand> cmd = UniquePtr<SetPropertyCommand>::create(allocator, editor, entity, j, "-source", buf, allocator);
					editor.executeCommand(cmd.move());
				}
				for (int k = 0, kc = scene->getPropertyCount(entity, j); k < kc; ++k) {
					char buf[256];
					const char* property_name = scene->getPropertyName(entity, j, k);
					if (!property_name) continue;
					scene->getPropertyValue(entity, j, property_name, buf, lengthOf(buf));
					switch (scene->getPropertyType(entity, j, k)) {
						case JSScriptScene::Property::BOOLEAN: {
							bool b = equalStrings(buf, "true");
							if (ImGui::Checkbox(property_name, &b)) {
								UniquePtr<SetPropertyCommand> cmd = UniquePtr<SetPropertyCommand>::create(allocator, editor, entity, j, property_name, b ? "true" : "false", allocator);
								editor.executeCommand(cmd.move());
							}
						} break;
						case JSScriptScene::Property::NUMBER: {
							float f = (float)atof(buf);
							if (ImGui::DragFloat(property_name, &f)) {
								toCString(f, Span(buf), 5);
								UniquePtr<SetPropertyCommand> cmd = UniquePtr<SetPropertyCommand>::create(allocator, editor, entity, j, property_name, buf, allocator);
								editor.executeCommand(cmd.move());
							}
						} break;
						case JSScriptScene::Property::STRING:
						case JSScriptScene::Property::ANY:
							if (ImGui::InputText(property_name, buf, sizeof(buf))) {
								UniquePtr<SetPropertyCommand> cmd = UniquePtr<SetPropertyCommand>::create(allocator, editor, entity, j, property_name, buf, allocator);
								editor.executeCommand(cmd.move());
							}
							break;
						default: ASSERT(false); break;
					}
				}
				if (auto* call = scene->beginFunctionCall(entity, j, "onGUI")) {
					scene->endFunctionCall();
				}
				ImGui::PopID();
			}
		}
	}

	StudioApp& m_app;
};


struct AssetBrowserPlugin : AssetBrowser::IPlugin {
	explicit AssetBrowserPlugin(StudioApp& app)
		: m_app(app) {
		m_text_buffer[0] = 0;
		app.getAssetCompiler().registerExtension("js", JSScript::TYPE);
	}

	ResourceType getResourceType() const override { return JSScript::TYPE; }

	void onGUI(Span<Resource*> resources) override {
		if (resources.length() > 1) return;

		auto* script = static_cast<JSScript*>(resources[0]);

		if (m_text_buffer[0] == '\0') {
			copyString(m_text_buffer, script->getSourceCode());
		}
		ImGui::InputTextMultiline("Code", m_text_buffer, sizeof(m_text_buffer), ImVec2(0, 300));
		if (ImGui::Button("Save")) {
			auto& fs = m_app.getWorldEditor().getEngine().getFileSystem();
			os::OutputFile file;
			if (!fs.open(script->getPath().c_str(), file)) {
				logWarning("Could not save ", script->getPath());
				return;
			}

			if (!file.write(m_text_buffer, stringLength(m_text_buffer))) {
				logWarning("Could not write ", script->getPath());
			}
			file.close();
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_EXTERNAL_LINK_ALT "Open externally")) {
			m_app.getAssetBrowser().openInExternalEditor(script->getPath().c_str());
		}
	}


	void onResourceUnloaded(Resource*) override { m_text_buffer[0] = 0; }
	const char* getName() const override { return "JS Script"; }


	StudioApp& m_app;
	char m_text_buffer[8192];
};


struct ConsolePlugin final : public StudioApp::GUIPlugin {
	ConsolePlugin(StudioApp& _app)
		: app(_app)
		, opened(false)
		, autocomplete(_app.getWorldEditor().getAllocator()) {
		open_action.init("JS console", "JS script console", "script_console", "", true);
		open_action.func.bind<&ConsolePlugin::toggleOpened>(this);
		open_action.is_selected.bind<&ConsolePlugin::isOpened>(this);
		app.addWindowAction(&open_action);
		buf[0] = '\0';
	}

	~ConsolePlugin() { app.removeAction(&open_action); }

	const char* getName() const override { return "script_console"; }


	bool isOpened() const { return opened; }
	void toggleOpened() { opened = !opened; }


	void autocompleteSubstep(duk_context* ctx, const char* str, ImGuiInputTextCallbackData* data) {
		char item[128];
		const char* next = str;
		char* c = item;
		while (*next != '.' && *next != '\0') {
			*c = *next;
			++next;
			++c;
		}
		*c = '\0';

		if (duk_is_null_or_undefined(ctx, -1)) return;

		duk_enum(ctx, -1, DUK_ENUM_INCLUDE_SYMBOLS | DUK_ENUM_INCLUDE_NONENUMERABLE);
		while (duk_next(ctx, -1, 0)) {
			/* [ ... enum key ] */
			if (duk_is_string(ctx, -1) && !duk_is_symbol(ctx, -1)) {
				const char* name = duk_to_string(ctx, -1);
				if (startsWith(name, item)) {
					if (*next == '.' && next[1] == '\0') {
						if (equalStrings(name, item)) {
							duk_get_prop_string(ctx, -3, name);
							autocompleteSubstep(ctx, "", data);
							duk_pop(ctx);
						}
					} else if (*next == '\0') {
						autocomplete.push(String(name, app.getWorldEditor().getAllocator()));
					} else {
						duk_get_prop_string(ctx, -3, name);
						autocompleteSubstep(ctx, next + 1, data);
						duk_pop(ctx);
					}
				}
			}
			duk_pop(ctx);
		}
		duk_pop(ctx);
	}


	static bool isWordChar(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }


	static int autocompleteCallback(ImGuiInputTextCallbackData* data) {
		auto* that = (ConsolePlugin*)data->UserData;
		WorldEditor& editor = that->app.getWorldEditor();
		auto* scene = static_cast<JSScriptScene*>(editor.getUniverse()->getScene("js_script"));
		if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
			duk_context* ctx = scene->getGlobalContext();

			int start_word = data->CursorPos;
			char c = data->Buf[start_word - 1];
			while (start_word > 0 && (isWordChar(c) || c == '.')) {
				--start_word;
				c = data->Buf[start_word - 1];
			}
			char tmp[128];
			copyNString(Span(tmp), data->Buf + start_word, data->CursorPos - start_word);

			that->autocomplete.clear();

			duk_push_global_object(ctx);
			that->autocompleteSubstep(ctx, tmp, data);
			duk_pop(ctx);
			if (!that->autocomplete.empty()) {
				that->open_autocomplete = true;
				qsort(&that->autocomplete[0], that->autocomplete.size(), sizeof(that->autocomplete[0]), [](const void* a, const void* b) {
					const char* a_str = ((const String*)a)->c_str();
					const char* b_str = ((const String*)b)->c_str();
					return compareString(a_str, b_str);
				});
			}
		} else if (that->insert_value) {
			int start_word = data->CursorPos;
			char c = data->Buf[start_word - 1];
			while (start_word > 0 && (isWordChar(c))) {
				--start_word;
				c = data->Buf[start_word - 1];
			}
			data->InsertChars(data->CursorPos, that->insert_value + data->CursorPos - start_word);
			that->insert_value = nullptr;
		}
		return 0;
	}


	void onWindowGUI() override {
		auto* scene = (JSScriptScene*)app.getWorldEditor().getUniverse()->getScene(JS_SCRIPT_TYPE);
		duk_context* context = scene->getGlobalContext();

		if (ImGui::Begin("JS Script console", &opened)) {
			if (ImGui::Button("Execute")) {
				duk_push_string(context, buf);
				if (duk_peval(context) != 0) {
					const char* error = duk_safe_to_string(context, -1);
					logError(error);
				}
				duk_pop(context);
			}
			ImGui::SameLine();
			if (ImGui::Button("Execute file")) {
				char tmp[LUMIX_MAX_PATH];
				if (os::getOpenFilename(Span(tmp), "Scripts\0*.JS\0", nullptr)) {
					os::InputFile file;
					IAllocator& allocator = app.getWorldEditor().getAllocator();
					if (file.open(tmp)) {
						size_t size = file.size();
						Array<char> data(allocator);
						data.resize((int)size + 1);
						if (file.read(data.begin(), size)) {
							data[(int)size] = '\0';
							duk_push_string(context, &data[0]);
							if (duk_peval(context) != 0) {
								const char* error = duk_safe_to_string(context, -1);
								logError(error);
							}
							duk_pop(context);
						}
						file.close();
					} else {
						logError("Failed to open file ", tmp);
					}
				}
			}
			if (insert_value) ImGui::SetKeyboardFocusHere();
			ImGui::InputTextMultiline("", buf, lengthOf(buf), ImVec2(-1, -1), ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackCompletion, autocompleteCallback, this);

			if (open_autocomplete) {
				ImGui::OpenPopup("autocomplete");
				ImGui::SetNextWindowPos(ImGuiEx::GetOsImePosRequest());
			}
			open_autocomplete = false;
			if (ImGui::BeginPopup("autocomplete")) {
				if (autocomplete.size() == 1) {
					insert_value = autocomplete[0].c_str();
				}
				if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow))) ++autocomplete_selected;
				if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow))) --autocomplete_selected;
				if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter))) insert_value = autocomplete[autocomplete_selected].c_str();
				if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) ImGui::CloseCurrentPopup();
				autocomplete_selected = clamp(autocomplete_selected, 0, autocomplete.size() - 1);
				for (int i = 0, c = autocomplete.size(); i < c; ++i) {
					const char* value = autocomplete[i].c_str();
					if (ImGui::Selectable(value, autocomplete_selected == i)) {
						insert_value = value;
					}
				}
				ImGui::EndPopup();
			}
		}
		ImGui::End();
	}


	StudioApp& app;
	Array<String> autocomplete;
	bool opened;
	bool open_autocomplete = false;
	int autocomplete_selected = 1;
	const char* insert_value = nullptr;
	char buf[10 * 1024];
	Action open_action;
};


IEditorCommand* createAddScriptCommand(WorldEditor& editor) {
	return LUMIX_NEW(editor.getAllocator(), PropertyGridPlugin::AddScriptCommand)(editor);
}


IEditorCommand* createSetPropertyCommand(WorldEditor& editor) {
	return LUMIX_NEW(editor.getAllocator(), PropertyGridPlugin::SetPropertyCommand)(editor);
}


IEditorCommand* createRemoveScriptCommand(WorldEditor& editor) {
	return LUMIX_NEW(editor.getAllocator(), PropertyGridPlugin::RemoveScriptCommand)(editor);
}


struct AddComponentPlugin final : public StudioApp::IAddComponentPlugin {
	AddComponentPlugin(StudioApp& _app)
		: app(_app) {}


	void onGUI(bool create_entity, bool, WorldEditor& editor) override {
		ImGui::SetNextWindowSize(ImVec2(300, 300));
		if (!ImGui::BeginMenu(getLabel())) return;
		char buf[LUMIX_MAX_PATH];
		AssetBrowser& asset_browser = app.getAssetBrowser();
		bool new_created = false;
		if (ImGui::Selectable("New")) {
			char full_path[LUMIX_MAX_PATH];
			if (os::getSaveFilename(Span(full_path), "JS script\0*.js\0", "js")) {
				os::OutputFile file;
				IAllocator& allocator = editor.getAllocator();
				if (file.open(full_path)) {
					new_created = editor.getEngine().getFileSystem().makeRelative(Span(buf), full_path);
					file.close();
				} else {
					logError("Failed to create ", buf);
				}
			}
		}
		bool create_empty = ImGui::Selectable("Empty", false);

		static StableHash32 selected_res_hash;
		if (asset_browser.resourceList(Span(buf), selected_res_hash, JSScript::TYPE, 0, false) || create_empty || new_created) {
			if (create_entity) {
				EntityRef entity = editor.addEntity();
				editor.selectEntities(Span(&entity, 1), false);
			}
			if (editor.getSelectedEntities().empty()) return;
			EntityRef entity = editor.getSelectedEntities()[0];

			if (!editor.getUniverse()->hasComponent(entity, JS_SCRIPT_TYPE)) {
				editor.addComponent(Span(&entity, 1), JS_SCRIPT_TYPE);
			}

			IAllocator& allocator = editor.getAllocator();
			auto cmd = UniquePtr<PropertyGridPlugin::AddScriptCommand>::create(allocator);

			auto* script_scene = static_cast<JSScriptScene*>(editor.getUniverse()->getScene(JS_SCRIPT_TYPE));
			cmd->scene = script_scene;
			cmd->cmp = entity;
			editor.executeCommand(cmd.move());

			if (!create_empty) {
				int scr_count = script_scene->getScriptCount(entity);
				auto set_source_cmd = UniquePtr<PropertyGridPlugin::SetPropertyCommand>::create(allocator, editor, entity, scr_count - 1, "-source", buf, allocator);
				editor.executeCommand(set_source_cmd.move());
			}

			ImGui::CloseCurrentPopup();
		}
		ImGui::EndMenu();
	}

	const char* getLabel() const override { return "JS Script"; }

	StudioApp& app;
};

} // namespace


LUMIX_STUDIO_ENTRY(js) {
	WorldEditor& editor = app.getWorldEditor();
	auto* cmp_plugin = LUMIX_NEW(editor.getAllocator(), AddComponentPlugin)(app);
	app.registerComponent("", "js_script", *cmp_plugin);

	auto* plugin = LUMIX_NEW(editor.getAllocator(), PropertyGridPlugin)(app);
	app.getPropertyGrid().addPlugin(*plugin);

	auto* asset_browser_plugin = LUMIX_NEW(editor.getAllocator(), AssetBrowserPlugin)(app);
	app.getAssetBrowser().addPlugin(*asset_browser_plugin);

	auto* console_plugin = LUMIX_NEW(editor.getAllocator(), ConsolePlugin)(app);
	app.addPlugin(*console_plugin);

	return nullptr;
}
