if plugin "js" then
	files { 
		"src/**.c",
		"src/**.cpp",
		"src/**.h",
		"genie.lua"
	}
	includedirs { "../../js/src", }
	defines { "BUILDING_JS" }
	dynamic_link_plugin { "engine" }
end