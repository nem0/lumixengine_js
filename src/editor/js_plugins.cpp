#define LUMIX_NO_CUSTOM_CRT
#include <string.h>
#include "../js_script_manager.h"
#include "../js_script_system.h"
#include "../duktape/duk_debugger.h"
#include "editor/asset_browser.h"
#include "editor/asset_compiler.h"
#include "editor/editor_asset.h"
#include "editor/property_grid.h"
#include "editor/settings.h"
#include "editor/studio_app.h"
#include "editor/utils.h"
#include "editor/world_editor.h"
#include "core/array.h"
#include "core/log.h"
#include "core/os.h"
#include "core/path.h"
#include "core/profiler.h"
#include "engine/engine.h"
#include "engine/file_system.h"
#include "engine/plugin.h"
#include "engine/reflection.h"
#include "engine/resource_manager.h"
#include "engine/world.h"
#include "imgui/imgui.h"



using namespace Lumix;


static const ComponentType JS_SCRIPT_TYPE = reflection::getComponentType("js_script");
#define JS_DEBUGGER

namespace {

static inline const u32 token_colors[] = {
	IM_COL32(0xFF, 0x00, 0xFF, 0xff),
	IM_COL32(0xe1, 0xe1, 0xe1, 0xff),
	IM_COL32(0xf7, 0xc9, 0x5c, 0xff),
	IM_COL32(0xFF, 0xA9, 0x4D, 0xff),
	IM_COL32(0xE5, 0x8A, 0xC9, 0xff),
	IM_COL32(0x93, 0xDD, 0xFA, 0xff),
	IM_COL32(0x67, 0x6b, 0x6f, 0xff),
	IM_COL32(0x67, 0x6b, 0x6f, 0xff)
};

enum class TokenType : u8 {
	EMPTY,
	IDENTIFIER,
	NUMBER,
	STRING,
	KEYWORD,
	OPERATOR,
	COMMENT,
	COMMENT_MULTI
};
	
static bool isWordChar(char c) {
	return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '_';
}

static bool tokenize(const char* str, u32& token_len, u8& token_type, u8 prev_token_type) {
	static const char* keywords[] = {
		"if",
		"then",
		"else",
		"elseif",
		"end",
		"do",
		"function",
		"repeat",
		"until",
		"while",
		"for",
		"break",
		"return",
		"local",
		"in",
		"not",
		"and",
		"or",
		"goto",
		"self",
		"true",
		"false",
		"nil"
	};

	const char* c = str;
	if (!*c) {
		token_type = prev_token_type == (u8)TokenType::COMMENT_MULTI ? (u8)TokenType::COMMENT_MULTI : (u8)TokenType::EMPTY;
		token_len = 0;
		return false;
	}

	if (prev_token_type == (u8)TokenType::COMMENT_MULTI) {
		token_type = (u8)TokenType::COMMENT;
		while (*c) {
			if (c[0] == '*' && c[1] == '/') {
				c += 2;
				token_len = u32(c - str);
				return *c;
			}
			++c;
		}
			
		token_type = (u8)TokenType::COMMENT_MULTI;
		token_len = u32(c - str);
		return *c;
	}

	if (c[0] == '/' && c[1] == '*') {
		while (*c) {
			if (c[0] == '*' && c[1] == '/') {
				c += 2;
				token_type = (u8)TokenType::COMMENT;
				token_len = u32(c - str);
				return *c;
			}
			++c;
		}
			
		token_type = (u8)TokenType::COMMENT_MULTI;
		token_len = u32(c - str);
		return *c;
	}


	if (*c == '/' && c[1] == '/') {
		token_type = (u8)TokenType::COMMENT;
		while (*c) ++c;
		token_len = u32(c - str);
		return *c;
	}

	if (*c == '"') {
		token_type = (u8)TokenType::STRING;
		++c;
		while (*c && *c != '"') ++c;
		if (*c == '"') ++c;
		token_len = u32(c - str);
		return *c;
	}

	if (*c == '\'') {
		token_type = (u8)TokenType::STRING;
		++c;
		while (*c && *c != '\'') ++c;
		if (*c == '\'') ++c;
		token_len = u32(c - str);
		return *c;
	}

	const char operators[] = "*/+-%.<>;=(),:[]{}&|^";
	for (char op : operators) {
		if (*c == op) {
			token_type = (u8)TokenType::OPERATOR;
			token_len = 1;
			return *c;
		}
	}
		
	if (*c >= '0' && *c <= '9') {
		token_type = (u8)TokenType::NUMBER;
		while (*c >= '0' && *c <= '9') ++c;
		token_len = u32(c - str);
		return *c;
	}

	if (*c >= 'a' && *c <= 'z' || *c >= 'A' && *c <= 'Z' || *c == '_') {
		token_type = (u8)TokenType::IDENTIFIER;
		while (isWordChar(*c)) ++c;
		token_len = u32(c - str);
		StringView token_view(str, str + token_len);
		for (const char* kw : keywords) {
			if (equalStrings(kw, token_view)) {
				token_type = (u8)TokenType::KEYWORD;
				break;
			}
		}
		return *c;
	}

	token_type = (u8)TokenType::IDENTIFIER;
	token_len = 1;
	++c;
	return *c;
}


struct EditorWindow : AssetEditorWindow {
	EditorWindow(const Path& path, StudioApp& app, IAllocator& allocator)
		: AssetEditorWindow(app)
		, m_allocator(allocator)
		, m_app(app)
	{
		m_resource = app.getEngine().getResourceManager().load<JSScript>(path);
	}

	~EditorWindow() {
		m_resource->decRefCount();
	}

	void save() {
		if (!m_code_editor) return;

		OutputMemoryStream blob(m_app.getAllocator());
		m_code_editor->serializeText(blob);
		m_app.getAssetBrowser().saveResource(*m_resource, blob);
		m_dirty = false;
	}
	
	void windowGUI() override {
		if (ImGui::BeginMenuBar()) {
			if (m_app.getCommonActions().save.iconButton(true, &m_app)) save();
			if (ImGuiEx::IconButton(ICON_FA_EXTERNAL_LINK_ALT, "Open externally")) m_app.getAssetBrowser().openInExternalEditor(m_resource);
			ImGui::EndMenuBar();
		}

		if (m_resource->isEmpty()) {
			ImGui::TextUnformatted("Loading...");
			return;
		}

		if (!m_code_editor) {
			m_code_editor = createCodeEditor(m_app);
			m_code_editor->setTokenColors(token_colors);
			m_code_editor->setTokenizer(tokenize);
			m_code_editor->setText(m_resource->getSourceCode());
		}

		ImGui::PushFont(m_app.getMonospaceFont());
		if (m_code_editor->gui("jseditor", ImVec2(0, 0), m_app.getMonospaceFont(), m_app.getDefaultFont())) {
			m_dirty = true;
		}
		ImGui::PopFont();
	}
	
	const Path& getPath() override { return m_resource->getPath(); }
	const char* getName() const override { return "JS script editor"; }

	IAllocator& m_allocator;
	StudioApp& m_app;
	JSScript* m_resource;
	UniquePtr<CodeEditor> m_code_editor;
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
	ResourceType getResourceType() const override { return JSScript::TYPE; }

	StudioApp& m_app;
};


struct ConsolePlugin final : public StudioApp::GUIPlugin {
	ConsolePlugin(StudioApp& _app)
		: m_app(_app)
		, m_autocomplete(_app.getWorldEditor().getAllocator())
	{
		m_buffer[0] = '\0';

		#ifdef JS_DEBUGGER
			logInfo("JS debugger listening");
			duk_debugger::init();
		#endif

		m_app.getSettings().registerOption("js_console_open", &m_is_open);
	}

	~ConsolePlugin() {
		duk_debugger::finish();
	}
	
	static void onDebuggerDetached(duk_context *ctx, void*) {
		duk_debugger::disconnect();
	} 

	void update(float) override {
		#ifdef JS_DEBUGGER
			JSScriptSystem* system = (JSScriptSystem*)m_app.getEngine().getSystemManager().getSystem("js_script");
			if (duk_debugger::isConnected()) {
				duk_debugger_cooperate(system->getGlobalContext());
			}
			else if (duk_debugger::tryConnect()) {
				logInfo("JS debugger connected");
				duk_debugger_attach(system->getGlobalContext(),
					duk_debugger::readCallback,
					duk_debugger::writeCallback,
					duk_debugger::peekCallback,
					nullptr,
					nullptr,
					nullptr,
					&ConsolePlugin::onDebuggerDetached,
					this);
				duk_debugger_cooperate(system->getGlobalContext());
			}
		#endif
	}

	const char* getName() const override { return "script_console"; }

	void autocompleteSubstep(duk_context* ctx, const char* str, ImGuiInputTextCallbackData* data) {
		StringView item;
		item.begin = str;
		item.end = str;
		while (*item.end != '.' && *item.end != '\0') {
			++item.end;
		}

		if (duk_is_null_or_undefined(ctx, -1)) return;

		duk_enum(ctx, -1, DUK_ENUM_INCLUDE_SYMBOLS | DUK_ENUM_INCLUDE_NONENUMERABLE);
		while (duk_next(ctx, -1, 0)) {
			/* [ ... enum key ] */
			if (duk_is_string(ctx, -1) && !duk_is_symbol(ctx, -1)) {
				const char* name = duk_to_string(ctx, -1);
				if (startsWith(name, item)) {
					if (*item.end == '.' && item.end[1] == '\0') {
						if (equalStrings(name, item)) {
							duk_get_prop_string(ctx, -3, name);
							autocompleteSubstep(ctx, "", data);
							duk_pop(ctx);
						}
					} else if (*item.end == '\0') {
						m_autocomplete.push(String(name, m_app.getWorldEditor().getAllocator()));
					} else {
						duk_get_prop_string(ctx, -3, name);
						autocompleteSubstep(ctx, item.end + 1, data);
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
		WorldEditor& editor = that->m_app.getWorldEditor();
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

			that->m_autocomplete.clear();

			duk_push_global_object(ctx);
			that->autocompleteSubstep(ctx, tmp, data);
			duk_pop(ctx);
			if (that->m_run_on_entity) {
				if (startsWith(tmp, "this.")) {
					Span<const EntityRef> selected = that->m_app.getWorldEditor().getSelectedEntities();
					if (selected.size() == 1 && module->getWorld().hasComponent(selected[0], JS_SCRIPT_TYPE)) {
						const uintptr id = module->getScriptID(selected[0], 0);
						duk_push_global_stash(ctx); // [stash]
						duk_push_pointer(ctx, (void*)id);  // [stash, id]
						duk_get_prop(ctx, -2); // [stash, this]
						duk_remove(ctx, -2); // [this]
						that->autocompleteSubstep(ctx, tmp + 5/*"this."*/, data);
						duk_pop(ctx);
					}
				}
				else if (startsWith("thi", tmp)) {
					that->m_autocomplete.push(String("this", that->m_app.getWorldEditor().getAllocator()));
				}
			}
			if (!that->m_autocomplete.empty()) {
				that->m_open_autocomplete = true;
				qsort(&that->m_autocomplete[0], that->m_autocomplete.size(), sizeof(that->m_autocomplete[0]), [](const void* a, const void* b) {
					const char* a_str = ((const String*)a)->c_str();
					const char* b_str = ((const String*)b)->c_str();
					return compareString(a_str, b_str);
				});
			}
		} else if (that->m_insert_value) {
			int start_word = data->CursorPos;
			char c = data->Buf[start_word - 1];
			while (start_word > 0 && (isWordChar(c))) {
				--start_word;
				c = data->Buf[start_word - 1];
			}
			data->InsertChars(data->CursorPos, that->m_insert_value + data->CursorPos - start_word);
			that->m_insert_value = nullptr;
		}
		return 0;
	}


	void onGUI() override {
		if (m_app.checkShortcut(m_open_action, true)) m_is_open = !m_is_open;
		if (!m_is_open) return;

		auto* module = (JSScriptModule*)m_app.getWorldEditor().getWorld()->getModule(JS_SCRIPT_TYPE);
		duk_context* context = module->getGlobalContext();
		if (ImGui::Begin("JavaScript console", &m_is_open)) {
			#ifdef JS_DEBUGGER
				bool is_connected = duk_debugger::isConnected();	
				ImGui::PushStyleColor(ImGuiCol_Text, is_connected ? IM_COL32(0, 0xff, 0, 0xff) : IM_COL32(0xff, 0, 0, 0xff));
				ImGui::Bullet();
				ImGui::PopStyleColor();
				ImGui::SetItemTooltip(is_connected ? "Debugger connected" : "Debugger disconnected");
				ImGui::SameLine(0, 16);
			#endif
			
			if (ImGui::Button("Execute")) {
				if (m_run_on_entity) {
					Span<const EntityRef> selected = m_app.getWorldEditor().getSelectedEntities();
					if (selected.size() != 1) logError("Exactly one entity must be selected");
					else {
						if (module->getWorld().hasComponent(selected[0], JS_SCRIPT_TYPE)) {
							module->execute(selected[0], 0, m_buffer);
						}
						else logError("Entity does not have JS component");
					}
				} else {
					duk_push_string(context, m_buffer);
					if (duk_peval(context) != 0) {
						const char* error = duk_safe_to_string(context, -1);
						logError(error);
					}
					duk_pop(context);
				}
			}
			ImGui::SameLine();
			ImGui::Checkbox("Run on entity", &m_run_on_entity);

			if (m_insert_value) ImGui::SetKeyboardFocusHere();
			ImGui::PushFont(m_app.getMonospaceFont());
			ImGui::InputTextMultiline("##buf", m_buffer, lengthOf(m_buffer), ImVec2(-1, -1), ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackCompletion, autocompleteCallback, this);
			ImGui::PopFont();

			if (m_open_autocomplete) {
				ImGui::OpenPopup("autocomplete");
				ImGui::SetNextWindowPos(ImGuiEx::GetOsImePosRequest());
			}
			m_open_autocomplete = false;
			if (ImGui::BeginPopup("autocomplete")) {
				if (m_autocomplete.size() == 1) {
					m_insert_value = m_autocomplete[0].c_str();
				}
				if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) ++m_autocomplete_selected;
				if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) --m_autocomplete_selected;
				if (ImGui::IsKeyPressed(ImGuiKey_Enter)) m_insert_value = m_autocomplete[m_autocomplete_selected].c_str();
				if (ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
				m_autocomplete_selected = clamp(m_autocomplete_selected, 0, m_autocomplete.size() - 1);
				for (int i = 0, c = m_autocomplete.size(); i < c; ++i) {
					const char* value = m_autocomplete[i].c_str();
					if (ImGui::Selectable(value, m_autocomplete_selected == i)) {
						m_insert_value = value;
					}
				}
				ImGui::EndPopup();
			}
		}
		ImGui::End();
	}


	StudioApp& m_app;
	Array<String> m_autocomplete;
	bool m_is_open = false;
	bool m_run_on_entity = false;
	bool m_open_autocomplete = false;
	i32 m_autocomplete_selected = 1;
	const char* m_insert_value = nullptr;
	char m_buffer[10 * 1024];
	Action m_open_action{"JavaScript", "JS console", "Console", "js_console",  nullptr, Action::WINDOW};
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
			file_selector.gui("js");
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
			if (editor.getSelectedEntities().size() == 0) return;
			EntityRef entity = editor.getSelectedEntities()[0];

			if (!editor.getWorld()->hasComponent(entity, JS_SCRIPT_TYPE)) {
				editor.addComponent(Span(&entity, 1), JS_SCRIPT_TYPE);
			}

			ComponentUID cmp;
			cmp.entity = entity;
			cmp.module = editor.getWorld()->getModule(JS_SCRIPT_TYPE);
			cmp.type = JS_SCRIPT_TYPE;
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
