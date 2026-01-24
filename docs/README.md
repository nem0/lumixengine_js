# JavaScript Plugin Documentation

This documentation covers the JavaScript scripting system for Lumix Engine, powered by [Duktape](https://duktape.org/).

## Contents

- [Engine API](engine_API.md) - Core API reference: entities, components, world, modules, ImGui, input handling
- [Properties](properties.md) - Script properties system: types, serialization, editor integration
- [Code Snippets](snippets.md) - Practical examples: movement, input, physics, state machines

## Quick Start

Create a `.js` file and attach it to an entity via the `js_script` component:

```javascript
var M = require("scripts/math");

({
    speed: 10.0,
    target: Lumix.INVALID_ENTITY,

    start: function() {
        Lumix.logError("Script started!");
    },

    update: function(td) {
        var pos = _entity.position;
        pos[0] += this.speed * td;
        _entity.position = pos;
    },

    onInputEvent: function(event) {
        if (event.key_id == 'W'.charCodeAt(0)) {
            Lumix.logError("W pressed: " + event.down);
        }
    }
})
```

## Features

- **Properties** - Variables are automatically exposed in the editor's property grid
- **Entity References** - Use `Lumix.INVALID_ENTITY` to create entity reference properties
- **Component Access** - Access components via `entity.component_name.property`
- **ImGui Integration** - Create debug UIs with the `ImGui` global object
- **Event Binding** - Subscribe to engine events with `Lumix.bindEvent()`
- **Modules** - Use `require()` to load external scripts
- **Debugging** - VS Code debugger support via [vscode-duktape-debug](https://github.com/harold-b/vscode-duktape-debug)

## Script Lifecycle

| Function | When Called |
|----------|-------------|
| `start()` | When the game starts (after all scripts loaded) |
| `update(td)` | Every frame (`td` = time delta in seconds) |
| `onStartGame()` | When game starts (before `start`) |
| `onDestroy()` | When the script is destroyed |
| `onInputEvent(event)` | For keyboard/mouse/controller input |
