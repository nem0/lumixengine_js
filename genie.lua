project "js"
	libType()
	files { 
		"src/**.c",
		"src/**.cpp",
		"src/**.h",
		"genie.lua"
	}
	includedirs { "../../js/src", }
	defines { "BUILDING_JS" }
	links { "engine" }
	useLua()
	defaultConfigurations()
