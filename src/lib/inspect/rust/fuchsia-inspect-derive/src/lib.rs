// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This crate provides types, traits and macros for ergonomic
//! interactions with `fuchsia_inspect`. Proc macros are originally defined
//! in a separate crate, but re-exported here. Users should depend directly
//! on this crate.

use core::fmt;
use core::ops::{Deref, DerefMut};
use fuchsia_inspect::{
    BoolProperty, BytesProperty, DoubleProperty, IntProperty, Node, Property, StringProperty,
    UintProperty,
};
use std::marker::PhantomData;

/// The `Unit` derive macro can be applied to named structs in order to generate an
/// implementation of the `Unit` trait. The name of the field corresponds to the
/// inspect node or property name, and the type of the field must also implement `Unit`.
/// Implementations of `Unit` are supplied for most primitives and `String`.
///
/// Example:
///
/// #[derive(Unit)]
/// struct Point {
///     x: f32,
///     y: f32,
/// }
pub use fuchsia_inspect_derive_macro::Unit;

/// Provides a custom inspect `fuchsia_inspect` subtree for a type which is
/// created, updated and removed in a single step. (It does NOT support per-field updates.)
pub trait Unit {
    /// This associated type owns a subtree (either a property or a node) of a parent inspect node.
    /// May be nested. When dropped, the subtree is detached from the parent.
    /// Default is required such that a detached state can be constructed. The base inspect node
    /// and property types implement default.
    type Data: Default;

    /// Insert an inspect subtree at `parent[name]` with values from `self` and return
    /// the inspect data.
    fn inspect_create<T: AsRef<str>>(&self, parent: &Node, name: T) -> Self::Data;

    /// Update the inspect subtree owned by the inspect data with values from self.
    fn inspect_update(&self, data: &mut Self::Data);
}

impl Unit for String {
    type Data = StringProperty;

    fn inspect_create<T: AsRef<str>>(&self, parent: &Node, name: T) -> Self::Data {
        parent.create_string(name, self)
    }

    fn inspect_update(&self, data: &mut Self::Data) {
        data.set(self);
    }
}

impl Unit for Vec<u8> {
    type Data = BytesProperty;

    fn inspect_create<T: AsRef<str>>(&self, parent: &Node, name: T) -> Self::Data {
        parent.create_bytes(name, &self)
    }

    fn inspect_update(&self, data: &mut Self::Data) {
        data.set(&self);
    }
}

/// Implement `Unit` for a primitive type. Some implementations result in a
/// non-lossy upcast in order to conform to the supported types in the inspect API.
///   `impl_t`: The primitive types to be implemented, e.g. `{ u8, u16 }`
///   `inspect_t`: The type the inspect API expects, e.g. `u64`
///   `prop_name`: The name the inspect API uses for functions, e.g. `uint`
///   `prop_name_cap`: The name the inspect API uses for types, e.g. `Uint`
macro_rules! impl_unit_primitive {
    ({ $($impl_t:ty), *}, $inspect_t:ty, $prop_name:ident, $prop_name_cap:ident) => {
        $(
            paste::item! {
                impl Unit for $impl_t {
                    type Data = [<$prop_name_cap Property>];

                    fn inspect_create<T: AsRef<str>>(&self, parent: &Node, name: T) -> Self::Data {
                        parent.[<create_ $prop_name>](name, *self as $inspect_t)
                    }

                    fn inspect_update(&self, data: &mut Self::Data) {
                        data.set(*self as $inspect_t);
                    }
                }
            }
        )*
    };
}

// Implement `Unit` for the supported primitive types.
impl_unit_primitive!({ u8, u16, u32, u64, usize }, u64, uint, Uint);
impl_unit_primitive!({ i8, i16, i32, i64, isize }, i64, int, Int);
impl_unit_primitive!({ f32, f64 }, f64, double, Double);
impl_unit_primitive!({ bool }, bool, bool, Bool);

/// Renders inspect state. This trait should be implemented with
/// a relevant constraint on the base type.
pub trait Render {
    /// The base type, provided by the user.
    type Base;

    /// Inspect data, provided by implementors of this trait.
    type Data: Default;

    /// Initializes the inspect data from the current state of base.
    fn create<T: AsRef<str>>(base: &Self::Base, parent: &Node, name: T) -> Self::Data;

    /// Updates the inspect data from the current state of base.
    fn update(base: &Self::Base, data: &mut Self::Data);
}

/// Generic smart pointer which owns an inspect subtree (either a Node or a
/// Property) for the duration of its lifetime. It dereferences to the
/// user-provided base type (similar to Arc and other smart pointers).
/// This type should rarely be used declared explictly. Rather, a specific smart
/// pointer (such as IValue) should be used.
pub struct IOwned<R: Render> {
    _base: R::Base,
    _inspect_data: R::Data,
}

impl<R: Render> IOwned<R> {
    /// Construct the smart pointer but don't populate any inspect state.
    pub fn new(value: R::Base) -> Self {
        let _inspect_data = R::Data::default();
        Self { _base: value, _inspect_data }
    }

    /// Construct the smart pointer and populate the inspect state under parent[name].
    pub fn attached(value: R::Base, parent: &Node, name: impl AsRef<str>) -> Self {
        let _inspect_data = R::create(&value, &parent, name);
        Self { _base: value, _inspect_data }
    }

    /// Explicitly update the inspect subtree.
    pub fn iupdate(&mut self) {
        R::update(&self._base, &mut self._inspect_data);
    }

    /// Set the value, then update inspect state.
    pub fn iset(&mut self, value: R::Base) {
        self._base = value;
        R::update(&self._base, &mut self._inspect_data);
    }

    pub fn into_inner(self) -> R::Base {
        self._base
    }
}

impl<R, B> fmt::Debug for IOwned<R>
where
    R: Render<Base = B>,
    B: fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(&self._base, f)
    }
}

impl<R, B> fmt::Display for IOwned<R>
where
    R: Render<Base = B>,
    B: fmt::Display,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Display::fmt(&self._base, f)
    }
}

impl<R, B> Default for IOwned<R>
where
    R: Render<Base = B>,
    B: Default,
{
    fn default() -> Self {
        let _inspect_data = R::Data::default();
        let _base = B::default();
        Self { _base, _inspect_data }
    }
}

impl<R: Render> Deref for IOwned<R> {
    type Target = R::Base;
    fn deref(&self) -> &Self::Target {
        &self._base
    }
}

impl<R: Render> DerefMut for IOwned<R> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self._base
    }
}

#[doc(hidden)]
pub struct ValueMarker<B: Unit>(PhantomData<B>);

impl<B: Unit> Render for ValueMarker<B> {
    type Base = B;
    type Data = B::Data;

    fn create<U: AsRef<str>>(base: &Self::Base, parent: &Node, name: U) -> Self::Data {
        base.inspect_create(parent, name)
    }

    fn update(base: &Self::Base, data: &mut Self::Data) {
        base.inspect_update(data);
    }
}

/// An `Inspect` smart pointer for a type `B`, which renders an
/// inspect subtree as defined by `B: Unit`.
pub type IValue<B> = IOwned<ValueMarker<B>>;

impl<B: Unit> From<B> for IValue<B> {
    fn from(value: B) -> Self {
        Self::new(value)
    }
}

#[doc(hidden)]
pub struct DebugMarker<B: fmt::Debug>(PhantomData<B>);

impl<B: fmt::Debug> Render for DebugMarker<B> {
    type Base = B;
    type Data = StringProperty;

    fn create<U: AsRef<str>>(base: &Self::Base, parent: &Node, name: U) -> Self::Data {
        parent.create_string(name, &format!("{:?}", base))
    }

    fn update(base: &Self::Base, data: &mut Self::Data) {
        data.set(&format!("{:?}", base));
    }
}

/// An `Inspect` smart pointer for a type `B`, which renders the debug
/// output of `B` as a string property.
pub type IDebug<B> = IOwned<DebugMarker<B>>;

impl<B: fmt::Debug> From<B> for IDebug<B> {
    fn from(value: B) -> Self {
        Self::new(value)
    }
}
