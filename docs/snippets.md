# Code Snippets

## Basic Script Structure

```javascript
// my_script.js
var M = require("scripts/math");  // Optional: import modules at top

({
    // Properties (visible in editor)
    speed: 10.0,
    isActive: true,
    target: Lumix.INVALID_ENTITY,

    // Called when the game starts (after all scripts are loaded)
    start: function() {
        Lumix.logError("Script started!");
    },

    // Called every frame, td = time delta in seconds
    update: function(td) {
        var pos = _entity.position;
        pos[0] += this.speed * td;
        _entity.position = pos;
    },

    // Called when the script is destroyed
    onDestroy: function() {
        Lumix.logError("Script destroyed!");
    },

    // Called when the game starts (before start)
    onStartGame: function() {
        Lumix.logError("Game started!");
    },

    // Called for input events
    onInputEvent: function(event) {
        if (event.type === Lumix.INPUT_EVENT_BUTTON && event.down) {
            Lumix.logError("Key pressed: " + event.key_id);
        }
    }
})
```

## Moving an Entity

```javascript
({
    speed: 5.0,

    update: function(td) {
        var pos = _entity.position;
        pos[2] += this.speed * td; // Move forward
        _entity.position = pos;
    }
})
```

## Rotating an Entity

```javascript
({
    rotationSpeed: 1.0,
    angle: 0,

    update: function(td) {
        this.angle += this.rotationSpeed * td;
        
        // Create a rotation quaternion around Y axis
        var halfAngle = this.angle * 0.5;
        var rotation = [0, Math.sin(halfAngle), 0, Math.cos(halfAngle)];
        _entity.rotation = rotation;
    }
})
```

## Math Utilities Module

```javascript
// scripts/math.js
({
    makeQuat: function(x, y, z, angle) {
        var s = Math.sin(angle / 2);
        return [x * s, y * s, z * s, Math.cos(angle / 2)];
    },
    mulVec3: function(a, s) {
        return [a[0] * s, a[1] * s, a[2] * s];
    },
    sub: function(a, b) {
        return [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
    },
    dot: function(a, b) {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    },
    normalized: function(a) {
        var len = Math.sqrt(this.dot(a, a));
        return this.mulVec3(a, 1 / len);
    }
})
```

## Handling Keyboard Input

```javascript
var is_w_down = false;
var is_s_down = false;
var is_a_down = false;
var is_d_down = false;

({
    moveSpeed: 10.0,

    onInputEvent: function(event) {
        // Use charCodeAt for key comparison
        if (event.key_id == 'W'.charCodeAt(0)) {
            is_w_down = event.down;
        }
        else if (event.key_id == 'S'.charCodeAt(0)) {
            is_s_down = event.down;
        }
        else if (event.key_id == 'A'.charCodeAt(0)) {
            is_a_down = event.down;
        }
        else if (event.key_id == 'D'.charCodeAt(0)) {
            is_d_down = event.down;
        }
    },

    update: function(td) {
        var velocity = [0, 0, 0];
        if (is_w_down) velocity[2] = this.moveSpeed;
        if (is_s_down) velocity[2] = -this.moveSpeed;
        if (is_a_down) velocity[0] = -this.moveSpeed;
        if (is_d_down) velocity[0] = this.moveSpeed;

        var pos = _entity.position;
        pos[0] += velocity[0] * td;
        pos[1] += velocity[1] * td;
        pos[2] += velocity[2] * td;
        _entity.position = pos;
    }
})
```

## Handling Mouse Input

```javascript
({
    sensitivity: 0.01,
    yaw: 0,
    pitch: 0,

    onInputEvent: function(event) {
        if (event.type !== Lumix.INPUT_EVENT_AXIS) return;
        if (event.device_type !== Lumix.INPUT_DEVICE_MOUSE) return;

        this.yaw += event.x * this.sensitivity;
        this.pitch += event.y * this.sensitivity;
        
        // Clamp pitch
        this.pitch = Math.max(-1.5, Math.min(1.5, this.pitch));
    },

    update: function(td) {
        // Apply rotation using quaternion
        var halfYaw = this.yaw * 0.5;
        _entity.rotation = [0, Math.sin(halfYaw), 0, Math.cos(halfYaw)];
    }
})
```

## Debug UI with ImGui

```javascript
({
    health: 100,
    showDebug: true,

    update: function(td) {
        if (!this.showDebug) return;

        ImGui.Begin("Debug Info");
        ImGui.Text("Health: " + this.health);
        this.health = ImGui.SliderFloat("Set Health", this.health, 0, 100);
        
        if (ImGui.Button("Reset")) {
            this.health = 100;
        }
        
        ImGui.End();
    }
})
```

## Physics-Based Vehicle Controller

Real-world example of a car controller using Jolt physics:

```javascript
var M = require("scripts/math");
var is_w_down = false;
var is_s_down = false;
var is_a_down = false;
var is_d_down = false;
var yaw = 0;

({
    mesh: Lumix.INVALID_ENTITY,
    phys: Lumix.INVALID_ENTITY,
    turn_speed: 1.5,
    debug: 0,

    start: function() {
        // Bind physics contact event
        Lumix.bindEvent(this.phys.world, "jolt", "onContact", function(e1, e2) {
            Lumix.logError("Contact at " + e1.position.toString());
        });
    },

    update: function(td) {
        var dir = [-Math.sin(yaw), 0, -Math.cos(yaw)];
        var speed = this.phys.jolt_body.Speed;

        // Acceleration
        if (is_w_down && !is_s_down) {
            this.phys.jolt_body.addForce(M.mulVec3(dir, 50000));
        }

        // Steering and friction
        if (speed > 0) {
            var vel = M.normalized(this.phys.jolt_body.LinearVelocity);
            var r = speed * 5000;
            this.phys.jolt_body.addForce(M.mulVec3(dir, r));
            this.phys.jolt_body.addForce(M.mulVec3(vel, -r));

            // Braking
            if (is_s_down && !is_w_down) {
                this.phys.jolt_body.addForce(M.mulVec3(vel, -25000));
            }
        }

        // Turning
        if (is_a_down) yaw += td * this.turn_speed * speed;
        if (is_d_down) yaw -= td * this.turn_speed * speed;

        // Sync mesh with physics body
        this.mesh.position = M.sub(this.phys.position, [0, 0.95, 0]);
        this.mesh.rotation = M.makeQuat(0, 1, 0, yaw);

        this.debug = speed;
    },

    onInputEvent: function(event) {
        if (event.key_id == 'W'.charCodeAt(0)) is_w_down = event.down;
        else if (event.key_id == 'S'.charCodeAt(0)) is_s_down = event.down;
        else if (event.key_id == 'A'.charCodeAt(0)) is_a_down = event.down;
        else if (event.key_id == 'D'.charCodeAt(0)) is_d_down = event.down;
    }
})
```

## Accessing Components

```javascript
({
    target: Lumix.INVALID_ENTITY,

    start: function() {
        // Access world through entity
        var world = _entity.world;
    },

    update: function(td) {
        // Access entity transform
        var pos = _entity.position;
        var rot = _entity.rotation;
        var scale = _entity.scale;
        
        // Modify position
        pos[1] = Math.sin(td) * 2.0;
        _entity.position = pos;

        // Access component on another entity
        if (this.target != Lumix.INVALID_ENTITY) {
            var targetPos = this.target.position;
        }
    }
})
```

## Timer and Delayed Actions

```javascript
({
    timer: 0,
    interval: 2.0,
    
    update: function(td) {
        this.timer += td;
        
        if (this.timer >= this.interval) {
            this.timer = 0;
            this.onInterval();
        }
    },
    
    onInterval: function() {
        Lumix.logError("Interval triggered!");
    }
})
```

## State Machine Pattern

```javascript
({
    state: "idle",
    stateTimer: 0,

    update: function(td) {
        this.stateTimer += td;
        
        switch (this.state) {
            case "idle":
                this.updateIdle(td);
                break;
            case "moving":
                this.updateMoving(td);
                break;
            case "attacking":
                this.updateAttacking(td);
                break;
        }
    },
    
    changeState: function(newState) {
        this.state = newState;
        this.stateTimer = 0;
    },
    
    updateIdle: function(td) {
        if (this.stateTimer > 3.0) {
            this.changeState("moving");
        }
    },
    
    updateMoving: function(td) {
        var pos = _entity.position;
        pos[0] += td;
        _entity.position = pos;
        
        if (this.stateTimer > 2.0) {
            this.changeState("idle");
        }
    },
    
    updateAttacking: function(td) {
        // Attack logic
    }
})
```
