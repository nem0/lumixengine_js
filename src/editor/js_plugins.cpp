#define LUMIX_NO_CUSTOM_CRT
#include "../js_script_manager.h"
#include "../js_script_system.h"
#include "editor/asset_browser.h"
#include "editor/asset_compiler.h"
#include "editor/editor_asset.h"
#include "editor/property_grid.h"
#include "editor/settings.h"
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
#include "engine/profiler.h"
#include "engine/reflection.h"
#include "engine/resource_manager.h"
#include "engine/world.h"
#include "imgui/imgui.h"
#include <cstdlib>


using namespace Lumix;


static const ComponentType JS_SCRIPT_TYPE = reflection::getComponentType("js_script");


namespace {


struct EditorWindow : AssetEditorWindow {
	EditorWindow(const Path& path, StudioApp& app, IAllocator& allocator)
		: AssetEditorWindow(app)
		, m_allocator(allocator)
		, m_buffer(allocator)
		, m_app(app)
	{
		m_resource = app.getEngine().getResourceManager().load<JSScript>(path);
	}

	~EditorWindow() {
		m_resource->decRefCount();
	}

	void save() {
		Span<const u8> data((const u8*)m_buffer.c_str(), m_buffer.length());
		m_app.getAssetBrowser().saveResource(*m_resource, data);
		m_dirty = false;
	}
	
	bool onAction(const Action& action) override { 
		if (&action == &m_app.getCommonActions().save) save();
		else return false;
		return true;
	}

	void windowGUI() override {
		if (ImGui::BeginMenuBar()) {
			if (ImGuiEx::IconButton(ICON_FA_SAVE, "Save")) save();
			if (ImGuiEx::IconButton(ICON_FA_EXTERNAL_LINK_ALT, "Open externally")) m_app.getAssetBrowser().openInExternalEditor(m_resource);
			ImGui::EndMenuBar();
		}

		if (m_resource->isEmpty()) {
			ImGui::TextUnformatted("Loading...");
			return;
		}

		if (m_buffer.length() == 0) m_buffer = m_resource->getSourceCode();

		ImGui::PushFont(m_app.getMonospaceFont());
		if (inputStringMultiline("##code", &m_buffer, ImGui::GetContentRegionAvail())) {
			m_dirty = true;
		}
		ImGui::PopFont();
	}
	
	const Path& getPath() override { return m_resource->getPath(); }
	const char* getName() const override { return "JS script editor"; }

	IAllocator& m_allocator;
	StudioApp& m_app;
	JSScript* m_resource;
	String m_buffer;
};

struct AssetPlugin : AssetBrowser::IPlugin, AssetCompiler::IPlugin {
	
	explicit AssetPlugin(StudioApp& app)
		: m_app(app)
	{
		app.getAssetCompiler().registerExtension("js", JSScript::TYPE);
	}

	void openEditor(const Path& path) override { 
		IAllocator& allocator = m_app.getAllocator();
		UniquePtr<EditorWindow> win = UniquePtr<EditorWindow>::create(allocator, path, m_app, m_app.getAllocator());
		m_app.getAssetBrowser().addWindow(win.move());
	}

	bool canCreateResource() const override { return true; }
	void createResource(OutputMemoryStream& content) override {}
	const char* getDefaultExtension() const override { return "js"; }
	bool compile(const Path& src) override { return m_app.getAssetCompiler().copyCompile(src); }
	const char* getLabel() const override { return "JS Script"; }

	StudioApp& m_app;
};


struct ConsolePlugin final : public StudioApp::GUIPlugin {
	ConsolePlugin(StudioApp& _app)
		: app(_app)
		, open(false)
		, autocomplete(_app.getWorldEditor().getAllocator()) {
		open_action.init("JS console", "JavaScript console", "script_console", "", true);
		open_action.func.bind<&ConsolePlugin::toggleOpen>(this);
		open_action.is_selected.bind<&ConsolePlugin::isOpen>(this);
		app.addWindowAction(&open_action);
		buf[0] = '\0';
	}

	~ConsolePlugin() { app.removeAction(&open_action); }

	const char* getName() const override { return "script_console"; }

	void onSettingsLoaded() override {
		Settings& settings = app.getSettings();
		open = settings.getValue(Settings::GLOBAL, "is_js_console_open", false);
		if (!buf[0]) {
			StringView dir = Path::getDir(settings.getAppDataPath());
			const StaticString<MAX_PATH> path(dir, "/js_console_content.lua");
			os::InputFile file;
			if (file.open(path)) {
				const u64 size = file.size();
				if (size + 1 <= sizeof(buf)) {
					if (!file.read(buf, size)) {
						logError("Failed to read ", path);
						buf[0] = '\0';
					}
					else {
						buf[size] = '\0';
					}
				}
				file.close();
			}
		}
	}

	void onBeforeSettingsSaved() override {
		Settings& settings = app.getSettings();
		settings.setValue(Settings::GLOBAL, "is_js_console_open", open);
		if (buf[0]) {
			StringView dir = Path::getDir(settings.getAppDataPath());
			const StaticString<MAX_PATH> path(dir, "/js_console_content.lua");
			os::OutputFile file;
			if (!file.open(path)) {
				logError("Failed to save ", path);
			}
			else {
				if (!file.write(buf, stringLength(buf))) {
					logError("Failed to write ", path);
				}
				file.close();
			}
		}
	}

	bool isOpen() const { return open; }
	void toggleOpen() { open = !open; }


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
		auto* module = static_cast<JSScriptModule*>(editor.getWorld()->getModule("js_script"));
		if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
			duk_context* ctx = module->getGlobalContext();

			int start_word = data->CursorPos;
			char c = data->Buf[start_word - 1];
			while (start_word > 0 && (isWordChar(c) || c == '.')) {
				--start_word;
				c = data->Buf[start_word - 1];
			}
			char tmp[128];
			copyString(Span(tmp), StringView(data->Buf + start_word, data->CursorPos - start_word));

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


	void onGUI() override {
		if (!open) return;

		auto* module = (JSScriptModule*)app.getWorldEditor().getWorld()->getModule(JS_SCRIPT_TYPE);
		duk_context* context = module->getGlobalContext();
		if (ImGui::Begin("JavaScript console", &open)) {
			if (ImGui::Button("Execute")) {
				if (run_on_entity) {
					const Array<EntityRef>& selected = app.getWorldEditor().getSelectedEntities();
					if (selected.size() != 1) logError("Exactly one entity must be selected");
					else {
						if (module->getWorld().hasComponent(selected[0], JS_SCRIPT_TYPE)) {
							module->execute(selected[0], 0, buf);
						}
						else logError("Entity does not have JS component");
					}
				} else {
					duk_push_string(context, buf);
					if (duk_peval(context) != 0) {
						const char* error = duk_safe_to_string(context, -1);
						logError(error);
					}
					duk_pop(context);
				}
			}
			ImGui::SameLine();
			ImGui::Checkbox("Run on entity", &run_on_entity);

			if (insert_value) ImGui::SetKeyboardFocusHere();
			ImGui::PushFont(app.getMonospaceFont());
			ImGui::InputTextMultiline("##buf", buf, lengthOf(buf), ImVec2(-1, -1), ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackCompletion, autocompleteCallback, this);
			ImGui::PopFont();

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
	bool open;
	bool run_on_entity = false;
	bool open_autocomplete = false;
	int autocomplete_selected = 1;
	const char* insert_value = nullptr;
	char buf[10 * 1024];
	Action open_action;
};


struct AddComponentPlugin final : public StudioApp::IAddComponentPlugin {
	AddComponentPlugin(StudioApp& app)
		: app(app)
		, file_selector("js", app)
	{}


	void onGUI(bool create_entity, bool, EntityPtr parent, WorldEditor& editor) override {
		if (!ImGui::BeginMenu(getLabel())) return;
		Path path;
		AssetBrowser& asset_browser = app.getAssetBrowser();
		bool new_created = false;
		if (ImGui::BeginMenu("New")) {
			file_selector.gui(false, "js");
			if (file_selector.getPath()[0] && ImGui::Selectable("Create")) {
				os::OutputFile file;
				FileSystem& fs = app.getEngine().getFileSystem();
				if (fs.open(file_selector.getPath(), file)) {
					file.close();
					new_created = true;
				} else {
					logError("Failed to create ", file_selector.getPath());
				}
				path = file_selector.getPath();
			}
			ImGui::EndMenu();
		}
		bool create_empty = ImGui::Selectable("Empty", false);

		static StableHash selected_res_hash;
		if (asset_browser.resourceList(path, selected_res_hash, JSScript::TYPE, false) || create_empty || new_created) {
			editor.beginCommandGroup("createEntityWithComponent");
			if (create_entity) {
				EntityRef entity = editor.addEntity();
				editor.selectEntities(Span(&entity, 1), false);
			}
			if (editor.getSelectedEntities().empty()) return;
			EntityRef entity = editor.getSelectedEntities()[0];

			if (!editor.getWorld()->hasComponent(entity, JS_SCRIPT_TYPE)) {
				editor.addComponent(Span(&entity, 1), JS_SCRIPT_TYPE);
			}

			const ComponentUID cmp = editor.getWorld()->getComponent(entity, JS_SCRIPT_TYPE);
			editor.addArrayPropertyItem(cmp, "scripts");

			if (!create_empty) {
				auto* script_scene = static_cast<JSScriptModule*>(editor.getWorld()->getModule(JS_SCRIPT_TYPE));
				int scr_count = script_scene->getScriptCount(entity);
				editor.setProperty(cmp.type, "scripts", scr_count - 1, "Path", Span((const EntityRef*)&entity, 1), path);
			}
			if (parent.isValid()) editor.makeParent(parent, entity);
			editor.endCommandGroup();
			editor.lockGroupCommand();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndMenu();
	}

	const char* getLabel() const override { return "JS Script"; }

	StudioApp& app;
	FileSelector file_selector;
};


struct StudioAppPlugin : StudioApp::IPlugin {
	StudioAppPlugin(StudioApp& app)
		: m_app(app)
		, m_asset_plugin(app)
		, m_console_plugin(app)
	{}

	~StudioAppPlugin() override {
		m_app.removePlugin(m_console_plugin);
		m_app.getAssetCompiler().removePlugin(m_asset_plugin);
		m_app.getAssetBrowser().removePlugin(m_asset_plugin);
	}

	void init() override {
		PROFILE_FUNCTION();
		WorldEditor& editor = m_app.getWorldEditor();
		auto* cmp_plugin = LUMIX_NEW(editor.getAllocator(), AddComponentPlugin)(m_app);
		m_app.registerComponent("", "js_script", *cmp_plugin);

		const char* exts[] = {"js"};
		m_app.getAssetCompiler().addPlugin(m_asset_plugin, Span(exts));
		m_app.getAssetBrowser().addPlugin(m_asset_plugin, Span(exts));
		m_app.addPlugin(m_console_plugin);
	}

	const char* getName() const override { return "js"; }

	bool showGizmo(struct WorldView& view, struct ComponentUID cmp) override { return false; }

	StudioApp& m_app;
	AssetPlugin m_asset_plugin;
	ConsolePlugin m_console_plugin;
};

} // namespace

LUMIX_STUDIO_ENTRY(js) {
	PROFILE_FUNCTION();
	IAllocator& allocator = app.getAllocator();
	return LUMIX_NEW(allocator, StudioAppPlugin)(app);
}
