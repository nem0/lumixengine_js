# lumixengine_js

JavaScript plugin for [Lumix Engine](https://github.com/nem0/LumixEngine)

This is just proof of concept and it's not supported.

Example JS code:

```js
function foo() {
	ImGui.Text("Hello world")
}

({
	name : "Test",
	update : function() {
		ImGui.Begin("xoxo")
		ImGui.Text("foo " + this.name)
		foo();
		ImGui.End()
	}
})
```
