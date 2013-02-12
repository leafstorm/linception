/**
 * linception.c
 *
 * Linception is a library for nesting Lua states inside other Lua states.
 *
 * (C) 2013 Matthew Frazier.
 * Released under the MIT/X11 license, see LICENSE for details.
 */

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"


/*** Lua initialization code ***/

#if LUA_VERSION_NUM < 502
#   define LUA_51 1
#   define luaL_newlib(L,l)         (lua_newtable(L), luaL_register(L, NULL, l))
#   define luaL_setfuncs(L,l,n)     luaL_register(L, NULL, l)
#else
#   define LUA_52 1
#   define define lua_setfenv(L,i)  lua_setupvalue(L, i, 1)
#   define lua_strlen(L,i)          lua_rawlen(L, (i))
#endif

int luaopen_linception (lua_State *L);


/*** Constants and names ***/

#define LINCEPTION_METATABLE    "linception.metatable"


/*** Data structures ***/

typedef struct linc_state_handle {
    lua_State *child;
} linc_state_handle;


/*** State mechanics ***/

/**
 * Creates a lua_State with the same allocator function as another
 * lua_State, so that their memory usage is linked.
 *
 * Returns NULL if a lua_State could not be allocated.
 */
static lua_State *linc_create_child_state (lua_State *parent) {
    lua_State *child;
    lua_Alloc allocator;
    void *userdata;

    allocator = lua_getallocf(parent, &userdata);
    child = lua_newstate(allocator, userdata);
    return child;
}


/*** State methods ***/

/**
 * Gets a state handle from the first argument provided ot the function.
 * If the state is not valid, Lua will say so.
 */
static linc_state_handle *linc_check_state_handle (lua_State *L) {
    linc_state_handle *sh =
        (linc_state_handle *)luaL_checkudata(L, 1, LINCEPTION_METATABLE);
    luaL_argcheck(L, sh != NULL, 1, "not a Lua state handle");
    luaL_argcheck(L, sh->child != NULL, 1, "state has been closed");
    return sh;
}


static const char *linc_library_names[] = {
    "base",
    LUA_COLIBNAME,
    LUA_MATHLIBNAME,
    LUA_STRLIBNAME,
    LUA_TABLIBNAME,
    LUA_IOLIBNAME,
    LUA_OSLIBNAME,
    LUA_LOADLIBNAME,
    LUA_DBLIBNAME,
    NULL
};

static const lua_CFunction linc_library_openers[] = {
    luaopen_base,
    luaopen_math,
    luaopen_string,
    luaopen_table,
    luaopen_io,
    luaopen_os,
    luaopen_package,
    luaopen_debug,
    NULL
};


/**
 * Opens a Lua standard library. Provide "base" or nil to open the base
 * library, or the name of a normal standard library like "math", "table",
 * "debug", etc. (Just be aware of what you're getting yourself into
 * if you allow certain libraries to a state.)
 *
 * Specifying an invalid library name is an error.
 */
static int linc_state_openlib (lua_State *L) {
    linc_state_handle *sh = linc_check_state_handle(L);
    lua_State *Lc = sh->child;

    int libindex = luaL_checkoption(L, 2, "base", linc_library_names);
    lua_CFunction opener = linc_library_openers[libindex];
    opener(Lc);

    return 0;
}


/**
 * Executes some code in a child state.
 */
static int linc_state_dostring (lua_State *L) {
    linc_state_handle *sh = linc_check_state_handle(L);
    lua_State *Lc = sh->child;

    size_t codelen;
    const char *code = luaL_checklstring(L, 2, &codelen);

    int load_error = luaL_loadbuffer(Lc, code, codelen, "parent");
    if (load_error == 0) {
        int error = lua_pcall(Lc, 0, LUA_MULTRET, 0);
        if (error) {
            lua_pushboolean(L, 0);
            lua_pushliteral(L, "could not run provided code");
            return 2;
        } else {
            lua_pushboolean(L, 1);
            return 1;
        }
    } else {
        lua_pushboolean(L, 0);
        lua_pushliteral(L, "could not compile provided code");
        return 2;
    }
}


/*** Library-level functions (linception.*) ***/

static int linc_newstate (lua_State *L) {
    linc_state_handle *sh;

    /* Allocate space for the state handle */
    sh = (linc_state_handle *)lua_newuserdata(L, sizeof(linc_state_handle));
    if (sh == NULL) {
        return luaL_error(L, "linception: could not allocate space for a "
                             "new state handle");
    }

    /* Create the Lua state */
    sh->child = NULL;
    luaL_getmetatable(L, LINCEPTION_METATABLE);
    lua_setmetatable(L, -2);
    sh->child = linc_create_child_state(L);
    if (sh->child == NULL) {
        return luaL_error(L, "linception: could not create new Lua state");
    }

    /* Return the state handle to Lua */
    return 1;
}


/*** Library initialization ***/

static int linc_install_metatable (lua_State *L) {
    struct luaL_Reg methods[] = {
        {"openlib",         linc_state_openlib},
        {"dostring",        linc_state_dostring},
        {NULL, NULL}
    };

    /* Register the metatable in the registry */
    if (!luaL_newmetatable(L, LINCEPTION_METATABLE)) {
        return 0;
    }

    /* Add normal methods */
    luaL_setfuncs(L, methods, 0);

    /* Add various magical keys */
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);

    return 1;
}


int luaopen_linception (lua_State *L) {
    /* Install library-level functions */
    struct luaL_Reg linception[] = {
        {"newstate",        linc_newstate},
        {NULL, NULL}
    };
    luaL_newlib(L, linception);

    /* Install metatable for state handles */
    linc_install_metatable(L);
    lua_pop(L, 1);

    /* Return the linception table (it's on top of the stack) */
    return 1;
}
