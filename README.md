# Linception

Linception is a Lua library for creating Lua states inside other Lua states.
This is useful in general for writing applications in Lua that deal with
Lua code, and need a more advanced view into the code they are running
or stronger sandboxing guarantees. It's also useful for myself, as an
opportunity to practice my C skills and learn the Lua API.

This differs from [Rings](http://keplerproject.github.com/rings/) by
providing more of the Lua API than just `dostring`, and providing stronger
isolation in the upward direction (i.e. the child doesn't have an arbitrary
`remotedostring` function).

The API is still taking shape, but eventually I imagine it looking like:

    local linception = require 'linception'

    local state = linception.newstate()
    state.globals.hello = state:proxy(function ()
        print "Hello, from the parent state!"
    end)

    state:call(state:global "hello")

I'm still deciding how much of the stack-based nature of the Lua API I
want to expose.


## Terminology

When you create a state using Linception, the newly created state is
referred to as the *child state*, and the state that created it, the *parent
state*. (In the C source, functions intended to be called from a parent
use `L` as the name of the `lua_State`, and functions intended to be called
from a child use `Lc`.)

Each state can have multiple children, but only one parent. Communication
from a parent to its child is referred to as *downward*, and communication
from a child to its parent is referred to as *upward*. Parents can only
communicate with their direct children, however, if the child state has
Linception installed, the parent can use the Lua API to manipulate the
state handles inside the child, and thus communicate indirectly.
