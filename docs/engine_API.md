# Engine API

The JS scripting plugin uses [Duktape](https://duktape.org/) as its JavaScript engine. Duktape is a lightweight, embeddable JavaScript engine with a focus on portability and compact footprint.

## Safety

The JS API is almost as unsafe as the C++ API, and most calls are not checked. This means calling methods on invalid objects or accessing properties on those objects can crash or overwrite random memory.

```javascript
var e = getSomeEntity();
// ...
e.position = [1, 2, 3]; // this is invalid if `e` is destroyed
```

## Script Structure

JS scripts should return an object literal using parentheses:

```javascript
({
    // Properties
    speed: 10.0,
    target: Lumix.INVALID_ENTITY,

    start: function() {
        // Called when game starts
    },

    update: function(td) {
        // Called every frame, td = time delta
    },

    onInputEvent: function(event) {
        // Handle input
    }
})
```

## Entity

Entities in JavaScript are proxy objects with the following properties and methods:

```javascript
// Entity properties
entity.position   // DVec3 - world position [x, y, z]
entity.rotation   // Quat - rotation as quaternion [x, y, z, w]
entity.scale      // Vec3 - scale [x, y, z]
```

### `_entity`

When inside an entity's script, the global `_entity` variable can be used to access the current entity:

```javascript
_entity.position = [1, 2, 3];
```

### Components

Components are accessed as properties of an entity. Component properties and methods are automatically exposed:

```javascript
({
    phys: Lumix.INVALID_ENTITY,  // Entity reference property

    update: function(td) {
        // Access component properties
        var speed = this.phys.jolt_body.Speed;
        var velocity = this.phys.jolt_body.LinearVelocity;

        // Call component methods
        this.phys.jolt_body.addForce([1000, 0, 0]);

        // Access entity transform
        var pos = this.phys.position;
        this.phys.rotation = [0, 0, 0, 1];
    }
})
```

Component properties are accessible through getter/setter pairs that are automatically generated from the engine's reflection system.

## Logging

```javascript
// Log error messages
Lumix.logError("Hello world");
```

## ImGui Integration

The JS plugin provides direct access to ImGui for creating debug UIs:

```javascript
ImGui.Begin("My Window");
ImGui.Text("Hello from JavaScript!");
if (ImGui.Button("Click me")) {
    Lumix.logError("Button clicked!");
}
var value = ImGui.SliderFloat("Speed", currentValue, 0.0, 100.0);
ImGui.End();
```

### Available ImGui Functions

- `Begin(name)` - Begin a window
- `End()` - End a window
- `Text(text)` - Display text
- `Button(label)` - Create a button, returns true if clicked
- `Checkbox(label, value)` - Create a checkbox, returns new value
- `CollapsingHeader(label)` - Create a collapsing header
- `Selectable(label, selected)` - Create a selectable item
- `SliderFloat(label, value, min, max)` - Float slider
- `DragFloat(label, value)` - Draggable float input
- `LabelText(label, text)` - Label with text
- `SameLine()` - Place next widget on same line
- `Separator()` - Horizontal separator
- `Columns(count)` - Begin columns layout
- `NextColumn()` - Move to next column
- `BeginPopup(name)` / `EndPopup()` - Popup handling
- `OpenPopup(name)` - Open a popup
- `BeginChildFrame(name, [width, height])` / `EndChild()` - Child frame
- `Indent()` / `Unindent()` - Indentation control
- `PushItemWidth(width)` / `PopItemWidth()` - Item width control
- `PushID(id)` / `PopID()` - ID stack management
- `PushStyleColor(...)` / `PopStyleColor()` - Style color control
- `PushStyleVar(...)` / `PopStyleVar()` - Style variable control
- `NewLine()` - Insert a new line
- `Dummy(size)` - Insert invisible spacing

## Input Events

Scripts can handle input events by implementing the `onInputEvent` function:

```javascript
return {
    onInputEvent: function(event) {
        // event.type - INPUT_EVENT_BUTTON, INPUT_EVENT_AXIS, INPUT_EVENT_TEXT_INPUT
        // event.device_type - INPUT_DEVICE_KEYBOARD, INPUT_DEVICE_MOUSE, INPUT_DEVICE_CONTROLLER
        // event.device_index - device index

        // For button events:
        // event.down - boolean, true if pressed
        // event.key_id - key identifier
        // event.is_repeat - boolean, true if key repeat
        // event.x, event.y - position (for mouse)

        // For axis events:
        // event.x, event.y - relative movement
        // event.x_abs, event.y_abs - absolute position

        // For text input:
        // event.text - UTF-8 text
    }
};
```

## Constants

The following constants are available in the `Lumix` global object:

```javascript
// Invalid entity reference
Lumix.INVALID_ENTITY

// Input event types
Lumix.INPUT_EVENT_BUTTON
Lumix.INPUT_EVENT_AXIS
Lumix.INPUT_EVENT_TEXT_INPUT

// Input device types
Lumix.INPUT_DEVICE_KEYBOARD
Lumix.INPUT_DEVICE_MOUSE
Lumix.INPUT_DEVICE_CONTROLLER
```

## Requiring External Scripts

Use the `require` function to load external JavaScript files:

```javascript
var M = require("scripts/math");

({
    update: function(td) {
        var dir = M.normalized([1, 0, 0]);
        var scaled = M.mulVec3(dir, 100);
    }
})
```

The `.js` extension is automatically appended to the path.

Modules should return an object literal:

```javascript
// scripts/math.js
({
    dot: function(a, b) {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    },
    mulVec3: function(a, s) {
        return [a[0] * s, a[1] * s, a[2] * s];
    },
    normalized: function(a) {
        var len = Math.sqrt(this.dot(a, a));
        return this.mulVec3(a, 1 / len);
    }
})
```
