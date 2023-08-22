# JavaScript plugin for [Lumix Engine](https://github.com/nem0/LumixEngine). 

* Attach scripts to entities
* All properties automatically accessible
* Console to execute scripts in global or entity's context, with autocomplete
* [Debugger](https://github.com/harold-b/vscode-duktape-debug) support
* Using [Duktape](https://github.com/svaarala/duktape)

Example JS code:

```js
function localFunction() {
	ImGui.Text("Hello world")
}

({
	name : "Test",
	entity : _entity,
	update : function() {
		this.name = "new name";
		this.entity.camera.fov = 1.2;
		ImGui.Begin("xoxo")
		ImGui.Text("foo " + this.name)
		localFunction();
		ImGui.End()
	}
})
```
![image](https://github.com/nem0/lumixengine_js/assets/153526/ad85998e-a028-4dcc-afe8-2e87f6c8a521)

VS Code debugger:

https://github.com/nem0/lumixengine_js/assets/153526/6fead5ad-0e2c-448b-a7dd-aaf59527998f

