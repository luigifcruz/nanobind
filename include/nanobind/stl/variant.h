/*
    nanobind/stl/variant.h: type caster for std::variant<...>

    Copyright (c) 2022 Yoshiki Matsuda and Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include <nanobind/nanobind.h>
#include <variant>

NAMESPACE_BEGIN(NB_NAMESPACE)
NAMESPACE_BEGIN(detail)

template <typename T, typename...>
struct concat_variant { using type = T; };
template <typename... Ts1, typename... Ts2, typename... Ts3>
struct concat_variant<std::variant<Ts1...>, std::variant<Ts2...>, Ts3...>
    : concat_variant<std::variant<Ts1..., Ts2...>, Ts3...> {};

template <typename... Ts> struct remove_opt_mono<std::variant<Ts...>>
    : concat_variant<std::conditional_t<std::is_same_v<std::monostate, Ts>, std::variant<>, std::variant<Ts>>...> {};

template <> struct type_caster<std::monostate> {
    NB_TYPE_CASTER(std::monostate, const_name("None"));

    bool from_python(handle src, uint8_t, cleanup_list *) noexcept {
        return src.is_none();
    }

    static handle from_cpp(const std::monostate &, rv_policy,
                           cleanup_list *) noexcept {
        return none().release();
    }
};

template <typename... Ts> struct type_caster<std::variant<Ts...>> {
    NB_TYPE_CASTER(std::variant<Ts...>,
        const_name("Union[") + concat(make_caster<Ts>::Name...) + const_name("]"));

    template <typename T>
    bool try_variant(const handle &src, uint8_t flags, cleanup_list *cleanup) {
        using CasterT = make_caster<T>;

        static_assert(
            !std::is_pointer_v<T> || is_base_caster_v<CasterT>,
            "Binding ``variant<T*, ...>`` requires that ``T`` is handled "
            "by nanobind's regular class binding mechanism. However, a "
            "type caster was registered to intercept this particular "
            "type, which is not allowed.");

        if (is_base_caster_v<CasterT> && !std::is_pointer_v<T>)
            flags |= (uint8_t) cast_flags::none_disallowed;

        CasterT caster;

        if (!caster.from_python(src, flags, cleanup))
            return false;

        value = caster.operator cast_t<T>();

        return true;
    }

    bool from_python(handle src, uint8_t flags, cleanup_list *cleanup) noexcept {
        return (try_variant<Ts>(src, flags, cleanup) || ...);
    }

    template <typename T>
    static handle from_cpp(T &&value, rv_policy policy, cleanup_list *cleanup) noexcept {
        return std::visit(
            [&](auto &&v) {
                return make_caster<decltype(v)>::from_cpp(
                    std::forward<decltype(v)>(v), policy, cleanup);
            },
            std::forward<T>(value));
    }
};

NAMESPACE_END(detail)
NAMESPACE_END(NB_NAMESPACE)
