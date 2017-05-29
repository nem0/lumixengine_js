project "lumixengine_js"
	libType()
	files { 
		"src/**.c",
		"src/**.cpp",
		"src/**.h",
		"genie.lua"
	}
	includedirs { "../../lumixengine_js/src", }
	defines { "BUILDING_JS" }
	links { "engine" }
	useLua()
	defaultConfigurations()
