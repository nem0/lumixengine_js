if plugin "js" then
	files { 
		"src/**.c",
		"src/**.cpp",
		"src/**.h",
		"genie.lua"
	}
	excludes { "src/meta_js.cpp" }
	includedirs { "../../js/src", }
	defines { "BUILDING_JS" }
	dynamic_link_plugin { "engine" }

	project "meta"
		files { "src/meta_js.cpp" }
end