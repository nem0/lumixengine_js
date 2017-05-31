#pragma once


#include "engine/log.h"
#include "engine/matrix.h"
#include "duktape/duktape.h"


namespace Lumix
{
namespace JSWrapper
{


template <typename T> struct ToType
{
	static const T& value(duk_context* ctx, int index)
	{
		void* ptr = duk_to_pointer(ctx, index);
		if (!ptr)
			duk_error(ctx, DUK_ERR_TYPE_ERROR, "Invalid argument %d - trying to convert null to reference", index);
		return *(T*)ptr;
	}
};

template <>
struct ToType<bool>
{
	static bool value(duk_context* ctx, int index)
	{
		return duk_to_boolean(ctx, index) != 0;
	}
};

template <>
struct ToType<float>
{
	static float value(duk_context* ctx, int index)
	{
		return (float)duk_to_number(ctx, index);
	}
};

template <>
struct ToType<const char*>
{
	static const char* value(duk_context* ctx, int index)
	{
		return duk_to_string(ctx, index);
	}
};

template <typename T>
struct ToType<T*>
{
	static T* value(duk_context* ctx, int index)
	{
		return (T*)duk_to_pointer(ctx, index);
	}
};

template <typename T>
struct ToType<T&>
{
	static T& value(duk_context* ctx, int index)
	{
		void* ptr = duk_to_pointer(ctx, index);
		if (!ptr) duk_error(ctx, DUK_ERR_TYPE_ERROR, "Invalid argument %d - trying to convert null to reference", index);
		return *(T*)ptr;
	}
};

template <>
struct ToType<Entity>
{
	static Entity value(duk_context* ctx, int index)
	{
		return {duk_to_int(ctx, index)};
	}
};

template <>
struct ToType<Vec3>
{
	static Vec3 value(duk_context* ctx, int index)
	{
		Vec3 v;
		duk_get_prop_index(ctx, index, 0);
		v.x = ToType<float>::value(ctx, -1);
		duk_get_prop_index(ctx, index, 1);
		v.y = ToType<float>::value(ctx, -1);
		duk_get_prop_index(ctx, index, 2);
		v.z = ToType<float>::value(ctx, -1);
		duk_pop_3(ctx);
		return v;
	}
};

template <>
struct ToType<Quat>
{
	static Quat value(duk_context* ctx, int index)
	{
		Quat v;
		duk_get_prop_index(ctx, index, 0);
		v.x = ToType<float>::value(ctx, -1);
		duk_get_prop_index(ctx, index, 1);
		v.y = ToType<float>::value(ctx, -1);
		duk_get_prop_index(ctx, index, 2);
		v.z = ToType<float>::value(ctx, -1);
		duk_get_prop_index(ctx, index, 3);
		v.w = ToType<float>::value(ctx, -1);
		duk_pop_n(ctx, 4);
		return v;
	}
};


template <typename T> inline T toType(duk_context* ctx, int index)
{
	return ToType<T>::value(ctx, index);
}
template <typename T> inline const char* typeToString()
{
	return "object";
}
template <> inline const char* typeToString<int>()
{
	return "integer";
}
template <> inline const char* typeToString<Entity>()
{
	return "entity";
}
template <> inline const char* typeToString<ComponentHandle>()
{
	return "component";
}
template <> inline const char* typeToString<u32>()
{
	return "integer";
}
template <> inline const char* typeToString<const char*>()
{
	return "string";
}
template <> inline const char* typeToString<bool>()
{
	return "boolean";
}
template <> inline const char* typeToString<float>()
{
	return "float";
}
template <> inline const char* typeToString<Vec3>()
{
	return "Vec3";
}
template <> inline const char* typeToString<Quat>()
{
	return "Quat";
}


template <typename T> inline bool isType(duk_context* ctx, int index)
{
	return duk_is_pointer(ctx, index) != 0;
}
template <> inline bool isType<int>(duk_context* ctx, int index)
{
	return duk_is_number(ctx, index) != 0;
}
template <> inline bool isType<Entity>(duk_context* ctx, int index)
{
	return duk_is_number(ctx, index) != 0;
}
template <> inline bool isType<ComponentHandle>(duk_context* ctx, int index)
{
	return duk_is_number(ctx, index) != 0;
}
template <> inline bool isType<u32>(duk_context* ctx, int index)
{
	return duk_is_number(ctx, index) != 0;
}
template <> inline bool isType<bool>(duk_context* ctx, int index)
{
	return duk_is_boolean(ctx, index) != 0;
}
template <> inline bool isType<float>(duk_context* ctx, int index)
{
	return duk_is_number(ctx, index) != 0;
}
template <> inline bool isType<const char*>(duk_context* ctx, int index)
{
	return duk_is_string(ctx, index) != 0;
}
template <> inline bool isType<void*>(duk_context* ctx, int index)
{
	return duk_is_pointer(ctx, index) != 0;
}
template <> inline bool isType<Vec3>(duk_context* ctx, int index)
{
	return duk_is_array(ctx, index) != 0;
}
template <> inline bool isType<Quat>(duk_context* ctx, int index)
{
	return duk_is_array(ctx, index) != 0;
}


template <typename T> inline void push(duk_context* ctx, T value)
{
	duk_push_pointer(ctx, value);
}
template <> inline void push(duk_context* ctx, float value)
{
	duk_push_number(ctx, value);
}
template <> inline void push(duk_context* ctx, double value)
{
	duk_push_number(ctx, value);
}
template <typename T> inline void push(duk_context* ctx, const T* value)
{
	duk_push_pointer(ctx, (T*)value);
}
template <> inline void push(duk_context* ctx, Entity value)
{
	duk_push_int(ctx, value.index);
}
template <> inline void push(duk_context* ctx, ComponentHandle value)
{
	duk_push_int(ctx, value.index);
}
template <> inline void push(duk_context* ctx, bool value)
{
	duk_push_boolean(ctx, value);
}
template <> inline void push(duk_context* ctx, const char* value)
{
	duk_push_string(ctx, value);
}
template <> inline void push(duk_context* ctx, char* value)
{
	duk_push_string(ctx, value);
}
template <> inline void push(duk_context* ctx, int value)
{
	duk_push_int(ctx, value);
}
template <> inline void push(duk_context* ctx, unsigned int value)
{
	duk_push_uint(ctx, value);
}
template <> inline void push(duk_context* ctx, void* value)
{
	duk_push_pointer(ctx, value);
}


inline void createSystemVariable(duk_context* ctx, const char* system, const char* var_name, void* value)
{
	// TODO
	/*if (lua_getglobal(ctx, system) == LUA_TNIL)
	{
		lua_pop(ctx, 1);
		lua_newtable(ctx);
		lua_setglobal(ctx, system);
		lua_getglobal(ctx, system);
	}
	lua_pushlightuserdata(ctx, value);
	lua_setfield(ctx, -2, var_name);
	lua_pop(ctx, 1);*/
}


inline void createSystemVariable(duk_context* ctx, const char* system, const char* var_name, int value)
{
	// TODO
	/*
	if (lua_getglobal(ctx, system) == LUA_TNIL)
	{
		lua_pop(ctx, 1);
		lua_newtable(ctx);
		lua_setglobal(ctx, system);
		lua_getglobal(ctx, system);
	}
	lua_pushinteger(ctx, value);
	lua_setfield(ctx, -2, var_name);
	lua_pop(ctx, 1);
	*/
}


inline void createSystemFunction(duk_context* ctx, const char* system, const char* var_name, duk_c_function fn)
{
	// TODO
	/*
	if (lua_getglobal(ctx, system) == LUA_TNIL)
	{
		lua_pop(ctx, 1);
		lua_newtable(ctx);
		lua_setglobal(ctx, system);
		lua_getglobal(ctx, system);
	}
	lua_pushcfunction(ctx, fn);
	lua_setfield(ctx, -2, var_name);
	lua_pop(ctx, 1);
	*/
}


inline void createSystemClosure(duk_context* ctx, const char* system, void* system_ptr, const char* var_name, duk_c_function fn)
{
	// TODO
	/*
	if (lua_getglobal(ctx, system) == LUA_TNIL)
	{
		lua_pop(ctx, 1);
		lua_newtable(ctx);
		lua_setglobal(ctx, system);
		lua_getglobal(ctx, system);
	}
	lua_pushlightuserdata(ctx, system_ptr);
	lua_pushcclosure(ctx, fn, 1);
	lua_setfield(ctx, -2, var_name);
	lua_pop(ctx, 1);
	*/
}



inline const char* jsTypeToString(duk_int_t type)
{
	switch (type)
	{
		case DUK_TYPE_NONE: return "none";
		case DUK_TYPE_UNDEFINED: return "undefined";
		case DUK_TYPE_NULL: return "null";
		case DUK_TYPE_BOOLEAN: return "boolean";
		case DUK_TYPE_NUMBER: return "number";
		case DUK_TYPE_STRING: return "string";
		case DUK_TYPE_OBJECT: return "object";
		case DUK_TYPE_BUFFER: return "buffer";
		case DUK_TYPE_POINTER: return "pointer";
		case DUK_TYPE_LIGHTFUNC: return "light func";
	}
	return "Unknown";
}


inline void argError(duk_context* ctx, int index, const char* expected_type)
{
	char buf[128];
	copyString(buf, "expected ");
	catString(buf, expected_type);
	catString(buf, ", got ");
	int type = duk_get_type(ctx, index);
	catString(buf, jsTypeToString(type));
	duk_error(ctx, DUK_ERR_TYPE_ERROR, buf);
}


template <typename T> void argError(duk_context* ctx, int index)
{
	argError(ctx, index, typeToString<T>());
}


template <typename T> T checkArg(duk_context* ctx, int index)
{
	if (!isType<T>(ctx, index))
	{
		argError<T>(ctx, index);
	}
	return toType<T>(ctx, index);
}

// TODO
/*
inline void checkTableArg(duk_context* ctx, int index)
{
	if (!lua_istable(ctx, index))
	{
		argError(ctx, index, "table");
	}
}
*/

template <typename T>
inline void getOptionalField(duk_context* ctx, int idx, const char* field_name, T* out)
{
	if (duk_get_prop(ctx, idx, field_name) && isType<T>(ctx, -1))
	{
		*out = toType<T>(ctx, -1);
	}
	duk_pop(ctx, 1);
}


namespace details
{


template <class T> struct remove_reference
{
	using type = T;
};

template <class T> struct remove_reference<T&>
{
	using type = T;
};

template <class T> struct remove_reference<T&&>
{
	using type = T;
};

template <class T> struct remove_const
{
	using type = T;
};

template <class T> struct remove_const<const T>
{
	using type = T;
};

template <class T> struct remove_volatile
{
	using type = T;
};

template <class T> struct remove_volatile<volatile T>
{
	using type = T;
};

template <class T> struct remove_cv
{
	using type = typename remove_const<typename remove_volatile<T>::type>::type;
};

template <class T> struct remove_cv_reference
{
	using type =  typename remove_const<typename remove_volatile<typename remove_reference<T>::type>::type>::type;
};


template <int... T>
struct Indices {};


template <int offset, int size, int... T>
struct build_indices
{
	using result = typename build_indices<offset, size - 1, size + offset, T...>::result;
};


template <int offset, int... T>
struct build_indices<offset, 0, T...>
{
	using result = Indices<T...>;
};


template <typename T, int index>
typename remove_cv_reference<T>::type convert(duk_context* ctx)
{
	return checkArg<typename remove_cv_reference<T>::type>(ctx, index - 1);
}


template <typename T> struct Caller;


template <int... indices>
struct Caller<Indices<indices...>>
{
	template <typename R, typename... Args>
	static int callFunction(R (*f)(Args...), duk_context* ctx)
	{
		R v = f(convert<Args, indices>(ctx)...);
		push(ctx, v);
		return 1;
	}


	template <typename... Args>
	static int callFunction(void (*f)(Args...), duk_context* ctx)
	{
		f(convert<Args, indices>(ctx)...);
		return 0;
	}


	template <typename R, typename... Args>
	static int callFunction(R(*f)(duk_context*, Args...), duk_context* ctx)
	{
		R v = f(ctx, convert<Args, indices>(ctx)...);
		push(ctx, v);
		return 1;
	}


	template <typename... Args>
	static int callFunction(void(*f)(duk_context*, Args...), duk_context* ctx)
	{
		f(ctx, convert<Args, indices>(ctx)...);
		return 0;
	}


	template <typename C, typename... Args>
	static int callMethod(C* inst, void(C::*f)(duk_context*, Args...), duk_context* ctx)
	{
		(inst->*f)(ctx, convert<Args, indices>(ctx)...);
		return 0;
	}


	template <typename R, typename C, typename... Args>
	static int callMethod(C* inst, R(C::*f)(duk_context*, Args...), duk_context* ctx)
	{
		R v = (inst->*f)(ctx, convert<Args, indices>(ctx)...);
		push(ctx, v);
		return 1;
	}


	template <typename R, typename C, typename... Args>
	static int callMethod(C* inst, R(C::*f)(duk_context*, Args...) const, duk_context* ctx)
	{
		R v = (inst->*f)(ctx, convert<Args, indices>(ctx)...);
		push(ctx, v);
		return 1;
	}


	template <typename C, typename... Args>
	static int callMethod(C* inst, void(C::*f)(Args...), duk_context* ctx)
	{
		(inst->*f)(convert<Args, indices>(ctx)...);
		return 0;
	}


	template <typename R, typename C, typename... Args>
	static int callMethod(C* inst, R(C::*f)(Args...), duk_context* ctx)
	{
		R v = (inst->*f)(convert<Args, indices>(ctx)...);
		push(ctx, v);
		return 1;
	}


	template <typename R, typename C, typename... Args>
	static int callMethod(C* inst, R(C::*f)(Args...) const, duk_context* ctx)
	{
		R v = (inst->*f)(convert<Args, indices>(ctx)...);
		push(ctx, v);
		return 1;
	}
};


template <typename R, typename... Args> constexpr int arity(R (*f)(Args...))
{
	return sizeof...(Args);
}


template <typename R, typename... Args> constexpr int arity(R (*f)(duk_context*, Args...))
{
	return sizeof...(Args);
}


template <typename R, typename C, typename... Args> constexpr int arity(R (C::*f)(Args...))
{
	return sizeof...(Args);
}


template <typename R, typename C, typename... Args> constexpr int arity(R(C::*f)(Args...) const)
{
	return sizeof...(Args);
}


template <typename R, typename C, typename... Args> constexpr int arity(R (C::*f)(duk_context*, Args...))
{
	return sizeof...(Args);
}


template <typename R, typename C, typename... Args> constexpr int arity(R (C::*f)(duk_context*, Args...) const)
{
	return sizeof...(Args);
}


} // namespace details


template <typename T, T t> int wrap(duk_context* ctx)
{
	using indices = typename details::build_indices<0, details::arity(t)>::result;
	return details::Caller<indices>::callFunction(t, ctx);
}


template <typename C, typename T, T t> int wrapMethod(duk_context* ctx)
{
	using indices = typename details::build_indices<0, details::arity(t)>::result;
	duk_push_this(ctx);
	duk_get_prop_string(ctx, -1, "c_ptr");
	auto* inst = toType<C*>(ctx, -1);
	duk_pop(ctx);
	return details::Caller<indices>::callMethod(inst, t, ctx);
}


// TODO
/*template <typename C, typename T, T t> int wrapMethodClosure(duk_context* ctx)
{
	using indices = typename details::build_indices<0, details::arity(t)>::result;
	int index = lua_upvalueindex(1);
	if (!isType<T>(ctx, index))
	{
		g_log_error.log("Lua") << "Invalid Lua closure";
		ASSERT(false);
		return 0;
	}
	auto* inst = checkArg<C*>(ctx, index);
	return details::Caller<indices>::callMethod(inst, t, L);
}*/


} // namespace JSWrapper
} // namespace Lumix
