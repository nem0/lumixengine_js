# lumixengine_js

JavaScript plugin for [Lumix Engine](https://github.com/nem0/LumixEngine)

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
