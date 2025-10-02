#pragma once


#include "core/log.h"
#include "core/math.h"
#include "core/metaprogramming.h"
#include "duktape/duktape.h"


namespace Lumix
{
namespace JSWrapper
{

#ifdef LUMIX_DEBUG
	struct LUMIX_ENGINE_API DebugGuard {
		DebugGuard(duk_context* ctx) : ctx(ctx) { top = duk_get_top(ctx); }
		DebugGuard(duk_context* ctx, i32 offset) : ctx(ctx) { top = duk_get_top(ctx) + offset; }
		~DebugGuard() {
			const duk_idx_t current_top = duk_get_top(ctx);
			ASSERT(current_top == top);
		}

	private:
		duk_context* ctx;
		duk_idx_t top;
	};
#else
	struct LUMIX_ENGINE_API DebugGuard { 
		DebugGuard(duk_context* ctx) {} 
		DebugGuard(duk_context* ctx, i32 offset) {} 
	};
#endif

template <typename T> T toType(duk_context* ctx, int index);

template <i32 NUM_ELEMENTS, typename T>
LUMIX_FORCE_INLINE void getVecN(duk_context* ctx, T* out, i32 object_index) {
	static_assert(NUM_ELEMENTS <= 4);
	ASSERT(object_index >= 0);

	if (duk_is_array(ctx, object_index)) {
		if (duk_get_length(ctx, object_index) != NUM_ELEMENTS) {
			duk_error(ctx, DUK_ERR_TYPE_ERROR, "not Vec%d, array must have %d elements", NUM_ELEMENTS, NUM_ELEMENTS);
		}
		for (i32 i = 0; i < NUM_ELEMENTS; ++i) {
			duk_get_prop_index(ctx, object_index, i);
			out[i] = toType<T>(ctx, -1);
		}
		duk_pop_n(ctx, NUM_ELEMENTS);
	}
	else {
		const char* chars[] = { "x", "y", "z", "w"};
		for (i32 i = 0; i < NUM_ELEMENTS; ++i) {
			if (!duk_get_prop_string(ctx, object_index, chars[i])) {
				duk_error(ctx, DUK_ERR_TYPE_ERROR, "not Vec%d, missing .%s", NUM_ELEMENTS, chars[i]);
			}
			out[i] = toType<T>(ctx, -1);
		}
		duk_pop_n(ctx, NUM_ELEMENTS);
	}
}

template <typename T> struct ToType {
	static const T& value(duk_context* ctx, int index) {
		void* ptr = duk_require_pointer(ctx, index);
		if (!ptr) duk_error(ctx, DUK_ERR_TYPE_ERROR, "trying to convert null pointer to reference");
		return *(T*)ptr;
	}
};

template <typename T> struct ToType<T*> {
	static T* value(duk_context* ctx, int index) { return (T*)duk_require_pointer(ctx, index); }
};

template <typename T> struct ToType<T&> {
	static T& value(duk_context* ctx, int index)
	{
		void* ptr = duk_require_pointer(ctx, index);
		if (!ptr) duk_error(ctx, DUK_ERR_TYPE_ERROR, "Invalid argument %d - trying to convert null to reference", index);
		return *(T*)ptr;
	}
};

template <> struct ToType<float> {
	static float value(duk_context* ctx, int index) {
		return (float)duk_require_number(ctx, index);
	}
};

template <> struct ToType<bool> {
	static bool value(duk_context* ctx, int index) {
		return duk_require_boolean(ctx, index);
	}
};

template <> struct ToType<i32> {
	static i32 value(duk_context* ctx, int index) {
		return duk_require_int(ctx, index);
	}
};

template <> struct ToType<u32> {
	static u32 value(duk_context* ctx, int index) {
		return duk_require_uint(ctx, index);
	}
};

template <> struct ToType<double> {
	static double value(duk_context* ctx, int index) {
		return duk_require_number(ctx, index);
	}
};

template <> struct ToType<const char*> {
	static const char* value(duk_context* ctx, int index) {
		return duk_require_string(ctx, index);
	}
};

template <> struct ToType<Path> {
	static Path value(duk_context* ctx, int index) {
		return Path(duk_require_string(ctx, index));
	}
};

template <> struct ToType<Vec3> {
	static Vec3 value(duk_context* ctx, int index) {
		Vec3 v;
		getVecN<3>(ctx, &v.x, index);
		return v;
	}
};

template <> struct ToType<Vec4> {
	static Vec4 value(duk_context* ctx, int index) {
		Vec4 v;
		getVecN<4>(ctx, &v.x, index);
		return v;
	}
};

template <> struct ToType<Quat> {
	static Quat value(duk_context* ctx, int index) {
		Quat v;
		getVecN<4>(ctx, &v.x, index);
		return v;
	}
};

template <> struct ToType<DVec3> {
	static DVec3 value(duk_context* ctx, int index) {
		DVec3 v;
		getVecN<3>(ctx, &v.x, index);
		return v;
	}
};

template <> struct ToType<IVec2> {
	static IVec2 value(duk_context* ctx, int index) {
		IVec2 v;
		getVecN<2>(ctx, &v.x, index);
		return v;
	}
};

template <> struct ToType<ImVec2> {
	static ImVec2 value(duk_context* ctx, int index) {
		ImVec2 v;
		getVecN<2>(ctx, &v.x, index);
		return v;
	}
};

template <> struct ToType<Vec2> {
	static Vec2 value(duk_context* ctx, int index) {
		Vec2 v;
		getVecN<2>(ctx, &v.x, index);
		return v;
	}
};

template <> struct ToType<EntityPtr> {
	static EntityPtr value(duk_context* ctx, int index) {
		if (!duk_get_prop_string(ctx, index, "c_entity")) {
			duk_error(ctx, DUK_ERR_TYPE_ERROR, "Expected entity");
		}
		if (!duk_is_number(ctx, -1)) {
			duk_error(ctx, DUK_ERR_TYPE_ERROR, "Expected entity");
		}
		i32 entity_index = ToType<i32>::value(ctx, -1);
		duk_pop(ctx);
		return EntityPtr{entity_index};
	}
};

template <> struct ToType<EntityRef> {
	static EntityRef value(duk_context* ctx, int index) {
		return *ToType<EntityPtr>::value(ctx, index);
	}
};

template <typename T> inline bool isType(duk_context* ctx, int index)
{
	return duk_is_pointer(ctx, index) != 0;
}
template <> inline bool isType<int>(duk_context* ctx, int index)
{
	return duk_is_number(ctx, index) != 0;
}
template <> inline bool isType<EntityPtr>(duk_context* ctx, int index)
{
	return duk_is_object(ctx, index) != 0;
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
template <> inline bool isType<double>(duk_context* ctx, int index)
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
template <> inline bool isType<Vec2>(duk_context* ctx, int index)
{
	return duk_is_array(ctx, index) != 0;
}
template <> inline bool isType<IVec2>(duk_context* ctx, int index)
{
	return duk_is_array(ctx, index) != 0;
}
template <> inline bool isType<Path>(duk_context* ctx, int index)
{
	return duk_is_string(ctx, index) != 0;
}
template <> inline bool isType<Quat>(duk_context* ctx, int index)
{
	return duk_is_array(ctx, index) != 0;
}


template <typename T> inline void push(duk_context* ctx, T* value)
{
	duk_push_pointer(ctx, value);
}
inline void pushEntity(duk_context* ctx, EntityPtr value, World* world) {
	duk_get_global_string(ctx, "Entity");
	duk_push_pointer(ctx, world);
	duk_push_int(ctx, value.index);
	duk_new(ctx, 2);
}
inline void push(duk_context* ctx, float value)
{
	duk_push_number(ctx, value);
}
inline void push(duk_context* ctx, double value)
{
	duk_push_number(ctx, value);
}
template <typename T> inline void push(duk_context* ctx, const T* value)
{
	duk_push_pointer(ctx, (T*)value);
}
inline void push(duk_context* ctx, bool value)
{
	duk_push_boolean(ctx, value);
}
inline void push(duk_context* ctx, const char* value)
{
	duk_push_string(ctx, value);
}
inline void push(duk_context* ctx, StringView value)
{
	duk_push_lstring(ctx, value.begin, value.size());
}
inline void push(duk_context* ctx, const Path& value)
{
	duk_push_string(ctx, value.c_str());
}
inline void push(duk_context* ctx, char* value)
{
	duk_push_string(ctx, value);
}
inline void push(duk_context* ctx, int value)
{
	duk_push_int(ctx, value);
}
inline void push(duk_context* ctx, unsigned int value)
{
	duk_push_uint(ctx, value);
}
inline void push(duk_context* ctx, void* value)
{
	duk_push_pointer(ctx, value);
}
inline void push(duk_context* ctx, const Vec3& value)
{
	duk_push_array(ctx);
	push(ctx, value.x);
	duk_put_prop_index(ctx, -2, 0);
	push(ctx, value.y);
	duk_put_prop_index(ctx, -2, 1);
	push(ctx, value.z);
	duk_put_prop_index(ctx, -2, 2);
}
inline void push(duk_context* ctx, const DVec3& value)
{
	duk_push_array(ctx);
	push(ctx, value.x);
	duk_put_prop_index(ctx, -2, 0);
	push(ctx, value.y);
	duk_put_prop_index(ctx, -2, 1);
	push(ctx, value.z);
	duk_put_prop_index(ctx, -2, 2);
}
inline void push(duk_context* ctx, const Vec4& value)
{
	duk_push_array(ctx);
	push(ctx, value.x);
	duk_put_prop_index(ctx, -2, 0);
	push(ctx, value.y);
	duk_put_prop_index(ctx, -2, 1);
	push(ctx, value.z);
	duk_put_prop_index(ctx, -2, 2);
	push(ctx, value.w);
	duk_put_prop_index(ctx, -2, 3);
}
inline void push(duk_context* ctx, const Vec2& value)
{
	duk_push_array(ctx);
	push(ctx, value.x);
	duk_put_prop_index(ctx, -2, 0);
	push(ctx, value.y);
	duk_put_prop_index(ctx, -2, 1);
}
inline void push(duk_context* ctx, const IVec2& value)
{
	duk_push_array(ctx);
	push(ctx, value.x);
	duk_put_prop_index(ctx, -2, 0);
	push(ctx, value.y);
	duk_put_prop_index(ctx, -2, 1);
}
inline void push(duk_context* ctx, const IVec3& value)
{
	duk_push_array(ctx);
	push(ctx, value.x);
	duk_put_prop_index(ctx, -2, 0);
	push(ctx, value.y);
	duk_put_prop_index(ctx, -2, 1);
	push(ctx, value.z);
	duk_put_prop_index(ctx, -2, 2);
}
inline void push(duk_context* ctx, const Quat& value)
{
	duk_push_array(ctx);
	push(ctx, value.x);
	duk_put_prop_index(ctx, -2, 0);
	push(ctx, value.y);
	duk_put_prop_index(ctx, -2, 1);
	push(ctx, value.z);
	duk_put_prop_index(ctx, -2, 2);
	push(ctx, value.w);
	duk_put_prop_index(ctx, -2, 3);
}

template <typename T> T toType(duk_context* ctx, int index) {
	return ToType<T>::value(ctx, index);
}

namespace details {

template <typename T, int index>
typename RemoveCVR<T> convert(duk_context* ctx)
{
	return toType<RemoveCVR<T>>(ctx, index - 1);
}

template <typename T> struct Caller;

template <int... indices>
struct Caller<Indices<indices...>> {
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

template <typename T, T t> int wrap(duk_context* ctx) {
	using indices = typename BuildIndices<0, details::arity(t)>::result;
	return details::Caller<indices>::callFunction(t, ctx);
}

template <typename T>
void setField(duk_context* ctx, const char* field_name, const T& value) {
	duk_push_string(ctx, field_name);
	push(ctx, value);
	duk_put_prop(ctx, -3);
}

} // namespace JSWrapper
} // namespace Lumix
