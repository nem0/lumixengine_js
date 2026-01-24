# Properties

Properties are variables in scripts that are visible in the property grid in the editor and can be accessed from outside their owning script. They are also serialized and deserialized with the component itself.

## Property Types

Properties can have the following types:

| Type | Description |
|------|-------------|
| `BOOLEAN` | Boolean value (`true` / `false`) |
| `NUMBER` | Floating-point number |
| `STRING` | Text string |
| `ENTITY` | Reference to an entity |

See `Property::Type` in [js_script_system.h](../src/js_script_system.h) for the full list.

## Defining Properties

Properties are detected automatically from the script's returned object. Non-function members become properties:

```javascript
({
    // These become properties
    speed: 10.0,           // NUMBER property
    isEnabled: true,       // BOOLEAN property
    targetEntity: Lumix.INVALID_ENTITY, // ENTITY property

    // Functions are not properties
    update: function(td) {
        // ...
    }
})
```

## Property Detection

The engine scans the returned object from a script and identifies properties based on their JavaScript type:

- `DUK_TYPE_BOOLEAN` → `Property::BOOLEAN`
- `DUK_TYPE_STRING` → `Property::STRING`  
- `DUK_TYPE_NUMBER` → `Property::NUMBER`
- Objects with `c_entity` member → `Property::ENTITY`

## Accessing Properties

Properties are accessible through the script instance and can be modified in the editor's property grid.

### From the Script

Properties defined in the returned object are accessible as `this` members within the script:

```javascript
({
    speed: 10.0,

    update: function(td) {
        // Access the property through this
        var currentSpeed = this.speed;
        this.speed = currentSpeed + 1;
    }
})
```

### Property Serialization

Properties are automatically saved and loaded with the scene. The stored values are preserved when:

- The scene is saved and reloaded
- The script is recompiled
- The game is stopped and started

## Entity Properties

Entity properties allow scripts to reference other entities in the scene:

```javascript
({
    target: Lumix.INVALID_ENTITY, // Initialize with invalid entity
    mesh: Lumix.INVALID_ENTITY,
    phys: Lumix.INVALID_ENTITY,

    update: function(td) {
        // Access properties on referenced entities
        if (this.target != Lumix.INVALID_ENTITY) {
            var targetPos = this.target.position;
            // ... use target position
        }

        // Sync mesh position with physics body
        this.mesh.position = this.phys.position;
    }
})
```

## Limitations

- A maximum of 256 * 8 = 2048 properties per script instance is supported
- If this limit is exceeded, a warning is logged and extra properties are ignored
- Blob properties and array properties are not currently supported in the JS scripting system
